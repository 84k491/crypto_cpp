#include "StrategyInstance.h"

#include "ByBitGateway.h"
#include "Position.h"

#include <chrono>
#include <optional>

StrategyInstance::StrategyInstance(
        const Symbol & symbol,
        const MarketDataRequest & md_request,
        const std::shared_ptr<IStrategy> & strategy_ptr,
        ByBitGateway & md_gateway,
        ByBitTradingGateway * tr_gateway)
    : m_md_gateway(md_gateway)
    , m_tr_gateway(tr_gateway)
    , m_strategy(strategy_ptr)
    , m_symbol(symbol)
    , m_position(symbol)
    , m_md_request(md_request)
{
    m_strategy_result.update([&](StrategyResult & res) {
        res.position_currency_amount = m_pos_currency_amount;
    });
}

void StrategyInstance::run_async()
{
    auto kline_sub_opt = m_md_gateway.subscribe_for_klines(
            m_symbol.symbol_name,
            [this](std::chrono::milliseconds ts, const OHLC & ohlc) {
                m_klines_publisher.push(ts, ohlc);
                const auto signal = m_strategy->push_price({ts, ohlc.close});
                if (signal.has_value()) {
                    on_signal(signal.value());
                }
            },
            m_md_request);
    if (kline_sub_opt == nullptr) {
        std::cout << "Failed to run strategy instance" << std::endl;
        return;
    }
    m_kline_sub = std::move(kline_sub_opt);
}

void StrategyInstance::stop_async()
{
    m_md_gateway.stop_async();
}

void StrategyInstance::wait_for_finish()
{
    m_md_gateway.wait_for_finish();

    if (m_position.opened() != nullptr) {
        const auto order_and_res_opt = m_position.close(m_last_signal.value().timestamp, m_last_signal.value().price);
        if (order_and_res_opt.has_value()) {
            const auto [order, res] = order_and_res_opt.value();
            m_strategy_result.update([&](StrategyResult & str_res) {
                str_res.final_profit += res.pnl;
            });
            m_depo_publisher.push(m_last_signal.value().timestamp, m_strategy_result.get().final_profit);
        }
    }
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
        m_strategy_result.update([&](StrategyResult & res) {
            res.final_profit += position_result_opt.value().pnl;
        });
        m_depo_publisher.push(signal.timestamp, m_strategy_result.get().final_profit);
        if (const auto best_profit = m_strategy_result.get().best_profit_trade;
            !best_profit.has_value() || best_profit < position_result_opt.value().pnl) {
            m_strategy_result.update([&](StrategyResult & res) {
                res.best_profit_trade = position_result_opt.value().pnl;
                res.longest_profit_trade_time =
                        std::chrono::duration_cast<std::chrono::seconds>(position_result_opt.value().opened_time);
            });
        }
        if (const auto worst_loss = m_strategy_result.get().worst_loss_trade;
            !worst_loss.has_value() || worst_loss > position_result_opt.value().pnl) {
            m_strategy_result.update([&](StrategyResult & res) {
                res.worst_loss_trade = position_result_opt.value().pnl;
                res.longest_loss_trade_time =
                        std::chrono::duration_cast<std::chrono::seconds>(position_result_opt.value().opened_time);
            });
        }
    }

    if (order_opt.has_value()) {
        if (m_tr_gateway != nullptr) {
            const auto success = m_tr_gateway->send_order_sync(order_opt.value());
            if (!success) {
                std::cout << "ERROR Failed to send order" << std::endl;
                return;
            }
            // const auto fee_paid = exec.execFee; // TODO

            // const auto slippage_delta = exec.execPrice - signal.price;
            // std::cout << "Slippage: " << slippage_delta << std::endl;
            // deposit -= fee_paid;
        }

        m_strategy_result.update([&](StrategyResult & res) {
            res.trades_count++;
            // res.fees_paid += fee_paid; // TODO
        });
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
