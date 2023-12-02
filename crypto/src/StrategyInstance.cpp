#include "StrategyInstance.h"

StrategyInstance::StrategyInstance(
        const Timerange & timerange,
        const DoubleSmaStrategyConfig & conf,
        ByBitGateway & md_gateway)
    : m_strategy(conf)
    , m_timerange(timerange)
    , m_md_gateway(md_gateway)
{
    m_md_gateway.subscribe_for_klines([&](std::pair<std::chrono::milliseconds, OHLC> ts_and_ohlc) {
        const auto & [ts, ohlc] = ts_and_ohlc;
        const auto signal = m_strategy.push_price({ts, ohlc.close});
        if (signal.has_value()) {
            on_signal(signal.value());
        }
    });
}

void StrategyInstance::run()
{
    m_md_gateway.receive_klines(m_timerange);

    if (m_last_signal.has_value()) {
        move_position_to(0, m_last_signal->price);
    }
    else {
        std::cout << "ERROR no signal on end" << std::endl;
    }
    m_strategy_result.profit = m_deposit;
}

void StrategyInstance::move_position_to(double to_position_size, double price)
{
    // make price optional for market orders

    // actual trade here
    const auto volume_delta = to_position_size - m_current_position_size;
    m_deposit += volume_delta * price;

    // fee

    m_current_position_size = to_position_size;
    m_strategy_result.trades++;
}

void StrategyInstance::on_signal(const Signal & signal)
{
    const int size_sign = signal.side == Side::Buy ? 1 : -1;
    const auto default_pos_size = m_pos_currency_amount / signal.price;
    move_position_to(size_sign * default_pos_size, signal.price);
    m_last_signal = signal;
    for (const auto & callback : m_signal_callbacks) {
        callback(signal);
    }
}

void StrategyInstance::subscribe_for_signals(std::function<void(const Signal &)> && on_signal_cb)
{
    m_signal_callbacks.emplace_back(std::move(on_signal_cb));
}

std::map<std::string, std::vector<std::pair<std::chrono::milliseconds, double>>>
StrategyInstance::get_strategy_internal_data_history() const
{
    return m_strategy.get_internal_data_history();
}

const StrategyResult & StrategyInstance::get_strategy_result() const
{
    return m_strategy_result;
}
