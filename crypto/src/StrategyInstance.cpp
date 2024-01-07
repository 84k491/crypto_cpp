#include "StrategyInstance.h"

#include "ByBitGateway.h"
#include "Position.h"

#include <chrono>
#include <optional>

StrategyInstance::StrategyInstance(
        const Timerange & timerange,
        const std::shared_ptr<IStrategy> & strategy_ptr,
        ByBitGateway & md_gateway)
    : m_md_gateway(md_gateway)
    , m_strategy(strategy_ptr)
    , m_timerange(timerange)
{
    m_strategy_result.update([&](StrategyResult & res) {
        res.position_currency_amount = m_pos_currency_amount;
    });
}

bool StrategyInstance::run(const Symbol & symbol)
{
    const bool success = m_md_gateway.get_klines(
            symbol.symbol_name,
            [this](std::pair<std::chrono::milliseconds, OHLC> ts_and_ohlc) {
                const auto & [ts, ohlc] = ts_and_ohlc;
                m_klines_publisher.push(ts, ohlc);
                const auto signal = m_strategy->push_price({ts, ohlc.close});
                if (signal.has_value()) {
                    on_signal(signal.value());
                }
            },
            m_timerange.start(),
            m_timerange.end());
    if (!success) {
        std::cout << "Failed to run strategy instance" << std::endl;
        return false;
    }

    if (m_position.opened() != nullptr) {
        const auto order_and_res_opt = m_position.close(m_last_signal.value().timestamp, m_last_signal.value().price);
        if (order_and_res_opt.has_value()) {
            const auto [order, res] = order_and_res_opt.value();
            m_deposit += res.pnl;
            m_depo_publisher.push(m_last_signal.value().timestamp, m_deposit);
        }
    }
    m_strategy_result.update([&](StrategyResult & res) {
        res.final_profit = m_deposit;
        res.profit_per_trade = res.final_profit / static_cast<double>(res.trades_count);
    });
    return true;
}

void StrategyInstance::on_signal(const Signal & signal)
{
    std::optional<MarketOrder> order_opt;
    std::optional<PositionResult> position_result_opt;
    if (Side::Close == signal.side) {
        if (m_position.opened() != nullptr) {
            const auto order_and_res_opt = m_position.close(signal.timestamp, signal.price);
            if (order_and_res_opt.has_value()) {
                std::tie(order_opt, position_result_opt) = order_and_res_opt.value();
            }
        }
    }
    else {
        const int size_sign = signal.side == Side::Buy ? 1 : -1;
        const auto default_pos_size = m_pos_currency_amount / signal.price;
        std::tie(order_opt, position_result_opt) =
                m_position.open_or_move(
                        signal.timestamp,
                        size_sign * default_pos_size,
                        signal.price);
    }

    if (position_result_opt.has_value()) {
        m_deposit += position_result_opt.value().pnl;
        m_depo_publisher.push(signal.timestamp, m_deposit);
        m_strategy_result.update([&](StrategyResult & res) {
            res.final_profit = m_deposit;
        });
        if (m_best_profit < position_result_opt.value().pnl) {
            m_best_profit = position_result_opt.value().pnl;
            m_strategy_result.update([&](StrategyResult & res) {
                res.best_profit_trade = position_result_opt.value().pnl;
                res.longest_profit_trade_time =
                        std::chrono::duration_cast<std::chrono::seconds>(position_result_opt.value().opened_time);
            });
        }
        if (m_worst_loss > position_result_opt.value().pnl) {
            m_worst_loss = position_result_opt.value().pnl;
            m_strategy_result.update([&](StrategyResult & res) {
                res.worst_loss_trade = position_result_opt.value().pnl;
                res.longest_loss_trade_time =
                        std::chrono::duration_cast<std::chrono::seconds>(position_result_opt.value().opened_time);
            });
        }
    }

    if (order_opt.has_value()) {
        // m_tr_gateway.place_order(order_opt.value());
        const auto fee_paid = ByBitGateway::get_taker_fee() * m_pos_currency_amount;
        m_deposit -= fee_paid;
        m_strategy_result.update([&](StrategyResult & res) {
            res.trades_count++;
            res.fees_paid += fee_paid;
        });
    }

    m_last_signal = signal;
    if (m_strategy_result.get().max_depo < m_deposit) {
        m_strategy_result.update([&](StrategyResult & res) {
            res.max_depo = m_deposit;
        });
    }
    if (m_strategy_result.get().min_depo > m_deposit) {
        m_strategy_result.update([&](StrategyResult & res) {
            res.min_depo = m_deposit;
        });
    }
    m_signal_publisher.push(signal.timestamp, signal);
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
