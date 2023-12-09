#include "StrategyInstance.h"

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
        move_position_to(0, m_last_signal->price);
    }
    else {
        std::cout << "ERROR no signal on end" << std::endl;
    }
    m_strategy_result.final_profit = m_deposit;
}

void StrategyInstance::move_position_to(double to_position_size, double price)
{
    // make price optional for market orders

    // actual trade here
    const auto volume_delta = to_position_size - m_current_position_size;
    m_deposit += volume_delta * price;

    // fee

    m_current_position_size = to_position_size;
    m_strategy_result.trades_count++;
}

void StrategyInstance::on_signal(const Signal & signal)
{
    const int size_sign = signal.side == Side::Buy ? 1 : -1;
    const auto default_pos_size = m_pos_currency_amount / signal.price;
    move_position_to(size_sign * default_pos_size, signal.price);
    m_last_signal = signal;
    for (const auto & depo_cb : m_depo_callbacks) {
        depo_cb(signal.timestamp, m_deposit);
    }
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
