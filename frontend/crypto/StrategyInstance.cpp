#include "StrategyInstance.h"

#include "ITradingGateway.h"
#include "Volume.h"

#include <chrono>
#include <optional>

StrategyInstance::StrategyInstance(
        const Symbol & symbol,
        const MarketDataRequest & md_request,
        const std::shared_ptr<IStrategy> & strategy_ptr,
        ByBitGateway & md_gateway,
        ITradingGateway & tr_gateway)
    : m_event_loop(*this)
    , m_md_gateway(md_gateway)
    , m_tr_gateway(tr_gateway)
    , m_strategy(strategy_ptr)
    , m_symbol(symbol)
    , m_position_manager(symbol, m_tr_gateway)
    , m_md_request(md_request)
{
    m_strategy_result.update([&](StrategyResult & res) {
        res.position_currency_amount = m_pos_currency_amount;
    });

    m_gw_status_sub = m_md_gateway.status_publisher().subscribe([this](const WorkStatus & status) {
        if (status == WorkStatus::Stopped || status == WorkStatus::Crashed) {
            if (m_position_manager.opened() != nullptr) {
                const auto position_result_opt = m_position_manager.close(m_last_price);
                if (position_result_opt.has_value()) {
                    const auto res = position_result_opt.value();
                    m_strategy_result.update([&](StrategyResult & str_res) {
                        str_res.final_profit += res.pnl;
                    });
                    m_depo_publisher.push(m_last_signal.value().timestamp, m_strategy_result.get().final_profit);
                }
            }
        }
    });
}

void StrategyInstance::run_with_loop()
{
}

void StrategyInstance::on_price_received(std::chrono::milliseconds ts, const OHLC & ohlc)
{
    m_last_price = ohlc.close;
    m_klines_publisher.push(ts, ohlc);
    if (!first_price_received) {
        m_depo_publisher.push(ts, 0.);
        first_price_received = true;
    }

    const auto signal = m_strategy->push_price({ts, ohlc.close});
    if (signal.has_value()) {
        on_signal(signal.value());
    }
}

void StrategyInstance::run_async()
{
    if (!m_md_request.go_live) {
        HistoricalMDRequest historical_request{
                .start = m_md_request.historical_range.value().start,
                .end = m_md_request.historical_range.value().end.value(),
                .symbol = m_symbol,
                .event_consumer = &m_event_loop,
        };
        m_md_gateway.push_async_request(std::move(historical_request));
    }
    else {
        LiveMDRequest live_request{
                .symbol = m_symbol,
                .event_consumer = &m_event_loop,
        };
        m_md_gateway.push_async_request(std::move(live_request));
    }
}

void StrategyInstance::stop_async()
{
    if (m_position_manager.opened() != nullptr) {
        std::cout << "Closing position on wait_for_finish" << std::endl;
        const auto position_result_opt = m_position_manager.close(m_last_price);
        if (position_result_opt.has_value()) {
            const auto res = position_result_opt.value();
            m_strategy_result.update([&](StrategyResult & str_res) {
                str_res.final_profit += res.pnl;
            });
            m_depo_publisher.push(m_last_signal.value().timestamp, m_strategy_result.get().final_profit);
        }
    }
    m_md_gateway.stop_async();
}

void StrategyInstance::wait_for_finish()
{
    m_md_gateway.wait_for_finish();
}

void StrategyInstance::on_signal(const Signal & signal)
{
    std::optional<PositionResult> position_result_opt;
    if (Side::Close == signal.side ||
        (m_position_manager.opened() != nullptr && signal.side != m_position_manager.opened()->side())) {
        if (m_position_manager.opened() != nullptr) {
            const auto pos_res_opt = m_position_manager.close(signal.price);
            if (!pos_res_opt.has_value()) {
                std::cout << "ERROR no result on position close" << std::endl;
            }
            position_result_opt = pos_res_opt;
            m_strategy_result.update([&](StrategyResult & res) {
                res.trades_count++;
            });
        }
    }

    if (m_position_manager.opened() != nullptr) {
        std::cout << "ERROR: position moving is not supported" << std::endl;
        return;
    }

    if (Side::Close != signal.side) {
        const auto default_pos_size_opt = UnsignedVolume::from(m_pos_currency_amount / signal.price);
        if (!default_pos_size_opt.has_value()) {
            std::cout << "ERROR can't get proper default position size" << std::endl;
            return;
        }
        const bool success = m_position_manager.open(
                signal.price,
                SignedVolume(
                        default_pos_size_opt.value(),
                        signal.side));
        if (!success) {
            std::cout << "ERROR Failed to open position" << std::endl;
            return;
        }
        m_strategy_result.update([&](StrategyResult & res) {
            res.trades_count++;
        });
    }

    if (position_result_opt.has_value()) {
        process_position_result(position_result_opt.value(), signal.timestamp);
    }

    m_last_signal = signal;
    if (m_strategy_result.get().max_depo < m_strategy_result.get().final_profit) {
        m_strategy_result.update([&](StrategyResult & res) {
            res.max_depo = res.final_profit;
        });
    }
    if (m_strategy_result.get().min_depo > m_strategy_result.get().final_profit) {
        m_strategy_result.update([&](StrategyResult & res) {
            res.min_depo = m_strategy_result.get().final_profit;
        });
    }
    m_signal_publisher.push(signal.timestamp, signal);
}

void StrategyInstance::process_position_result(const PositionResult & new_result, std::chrono::milliseconds ts)
{
    m_strategy_result.update([&](StrategyResult & res) {
        res.final_profit += new_result.pnl;
        res.fees_paid += new_result.fees_paid;
    });
    m_depo_publisher.push(ts, m_strategy_result.get().final_profit);
    if (const auto best_profit = m_strategy_result.get().best_profit_trade;
        !best_profit.has_value() || best_profit < new_result.pnl) {
        m_strategy_result.update([&](StrategyResult & res) {
            res.best_profit_trade = new_result.pnl;
            res.longest_profit_trade_time =
                    std::chrono::duration_cast<std::chrono::seconds>(new_result.opened_time);
        });
    }
    if (const auto worst_loss = m_strategy_result.get().worst_loss_trade;
        !worst_loss.has_value() || worst_loss > new_result.pnl) {
        m_strategy_result.update([&](StrategyResult & res) {
            res.worst_loss_trade = new_result.pnl;
            res.longest_loss_trade_time =
                    std::chrono::duration_cast<std::chrono::seconds>(new_result.opened_time);
        });
    }
}

TimeseriesPublisher<Signal> & StrategyInstance::signals_publisher()
{
    return m_signal_publisher;
}

TimeseriesPublisher<std::pair<std::string, double>> & StrategyInstance::strategy_internal_data_publisher()
{
    return m_strategy->strategy_internal_data_publisher();
}

TimeseriesPublisher<OHLC> & StrategyInstance::klines_publisher()
{
    return m_klines_publisher;
}

TimeseriesPublisher<double> & StrategyInstance::depo_publisher()
{
    return m_depo_publisher;
}

ObjectPublisher<StrategyResult> & StrategyInstance::strategy_result_publisher()
{
    return m_strategy_result;
}

ObjectPublisher<WorkStatus> & StrategyInstance::status_publisher()
{
    return m_md_gateway.status_publisher();
}

void StrategyInstance::invoke(const MDResponseEvent & value)
{
    if (const auto * r = std::get_if<MDPriceEvent>(&value); r) {
        std::cout << "Got live price response" << std::endl;
        const MDPriceEvent & response = *r;
        on_price_received(response.first, response.second);
        return;
    }
    if (const auto * r = std::get_if<HistoricalMDPackEvent>(&value); r) {
        std::cout << "Got historical price response" << std::endl;
        const HistoricalMDPackEvent & response = *r;
        std::cout << "Request size: " << response.size() << std::endl;
        for (const auto & [ts, ohlc] : response) {
            on_price_received(ts, ohlc);
        }
        return;
    }
    std::cout << "ERROR: Unhandled MDResponseEvent" << std::endl;
}
