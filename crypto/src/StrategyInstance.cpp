#include "StrategyInstance.h"

#include "Position.h"

#include <chrono>

StrategyInstance::StrategyInstance(
        const Timerange & timerange,
        const DoubleSmaStrategyConfig & conf,
        ByBitGateway & md_gateway)
    : m_strategy(conf)
    , m_timerange(timerange)
    , m_md_gateway(md_gateway)
{
    m_strategy.subscribe_for_strategy_internal([this](const std::string & name,
                                                      std::chrono::milliseconds ts,
                                                      double data) {
        for (const auto & cb : m_strategy_internal_callbacks) {
            cb(name, ts, data);
        }
    });

    m_strategy_result.position_currency_amount = m_pos_currency_amount;
}

void StrategyInstance::subscribe_for_strategy_internal(
        std::function<void(std::string name,
                           std::chrono::milliseconds ts,
                           double data)> && cb)
{
    m_strategy_internal_callbacks.emplace_back(std::move(cb));
}

void StrategyInstance::run()
{
    m_md_gateway.get_klines(
            "ETHUSDT",
            m_timerange,
            [this](std::pair<std::chrono::milliseconds, OHLC> ts_and_ohlc) {
                const auto & [ts, ohlc] = ts_and_ohlc;
                for (const auto & cb : m_kline_callbacks) {
                    cb(ts_and_ohlc);
                }
                const auto signal = m_strategy.push_price({ts, ohlc.close});
                if (signal.has_value()) {
                    on_signal(signal.value());
                }
            });

    if (m_last_signal.has_value()) {
        const auto order_and_res_opt = m_position.close(m_last_signal.value().timestamp, m_last_signal.value().price);
        if (order_and_res_opt.has_value()) {
            const auto [order, res] = order_and_res_opt.value();
            m_deposit += res.pnl;
            for (const auto & depo_cb : m_depo_callbacks) {
                depo_cb(m_last_signal.value().timestamp, m_deposit);
            }
        }
    }
    else {
        std::cout << "ERROR no signal on end" << std::endl;
    }
    m_strategy_result.final_profit = m_deposit;
    m_strategy_result.profit_per_trade = m_strategy_result.final_profit / m_strategy_result.trades_count;
}

void StrategyInstance::on_signal(const Signal & signal)
{
    const int size_sign = signal.side == Side::Buy ? 1 : -1;
    const auto default_pos_size = m_pos_currency_amount / signal.price;
    const auto [order_opt, position_result_opt] =
            m_position.open_or_move(
                    signal.timestamp,
                    size_sign * default_pos_size,
                    signal.price);

    if (position_result_opt.has_value()) {
        m_deposit += position_result_opt.value().pnl;
        for (const auto & depo_cb : m_depo_callbacks) {
            depo_cb(signal.timestamp, m_deposit);
        }
        m_strategy_result.final_profit = m_deposit;
        if (m_best_profit < position_result_opt.value().pnl) {
            m_best_profit = position_result_opt.value().pnl;
            m_strategy_result.best_profit_trade = position_result_opt.value().pnl;
            m_strategy_result.longest_profit_trade_time =
                    std::chrono::duration_cast<std::chrono::seconds>(position_result_opt.value().opened_time);
        }
        if (m_worst_loss > position_result_opt.value().pnl) {
            m_worst_loss = position_result_opt.value().pnl;
            m_strategy_result.worst_loss_trade = position_result_opt.value().pnl;
            m_strategy_result.longest_loss_trade_time =
                    std::chrono::duration_cast<std::chrono::seconds>(position_result_opt.value().opened_time);
        }
    }

    if (order_opt.has_value()) {
        // m_md_gateway.place_order(order_opt.value());
        m_strategy_result.trades_count++;
    }

    m_last_signal = signal;
    if (m_strategy_result.max_depo < m_deposit) {
        m_strategy_result.max_depo = m_deposit;
    }
    if (m_strategy_result.min_depo > m_deposit) {
        m_strategy_result.min_depo = m_deposit;
    }
    for (const auto & callback : m_signal_callbacks) {
        callback(signal);
    }
}

void StrategyInstance::subscribe_for_signals(std::function<void(const Signal &)> && on_signal_cb)
{
    m_signal_callbacks.emplace_back(std::move(on_signal_cb));
}

const StrategyResult & StrategyInstance::get_strategy_result() const
{
    return m_strategy_result;
}

void StrategyInstance::subscribe_for_klines(KlineCallback && on_kline_received_cb)
{
    m_kline_callbacks.emplace_back(std::move(on_kline_received_cb));
}

void StrategyInstance::subscribe_for_depo(DepoCallback && on_depo_cb)
{
    m_depo_callbacks.emplace_back(std::move(on_depo_cb));
}