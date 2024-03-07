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
                const bool success = close_position(m_last_ts_and_price.second, m_last_ts_and_price.first);
                if (!success) {
                    std::cout << "ERROR: can't close a position on stop/crash" << std::endl;
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
    m_last_ts_and_price = {ts, ohlc.close};
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
        HistoricalMDRequest historical_request(
                m_event_loop,
                m_symbol,
                m_md_request.historical_range.value().start,
                m_md_request.historical_range.value().end.value());
        m_md_gateway.push_async_request(std::move(historical_request));
    }
    else {
        LiveMDRequest live_request(
                m_event_loop,
                m_symbol);
        m_md_gateway.push_async_request(std::move(live_request));
    }
}

void StrategyInstance::stop_async()
{
    if (m_position_manager.opened() != nullptr) {
        std::cout << "Closing position on wait_for_finish" << std::endl;
        const bool success = close_position(m_last_ts_and_price.second, m_last_ts_and_price.first);
        if (!success) {
            std::cout << "ERROR: can't close a position on stop" << std::endl;
        }
    }

    m_md_gateway.stop_async(); // TODO "unsubscribe" from GW, but don't stop it
}

void StrategyInstance::wait_for_finish()
{
    m_md_gateway.wait_for_finish();
}

void StrategyInstance::on_signal(const Signal & signal)
{
    // closing if close or flip
    if (Side::Close == signal.side ||
        (m_position_manager.opened() != nullptr && signal.side != m_position_manager.opened()->side())) {
        if (m_position_manager.opened() != nullptr) {
            const bool success = close_position(signal.price, signal.timestamp);
            if (!success) {
                std::cout << "ERROR failed to close a position" << std::endl;
            }
        }
    }

    // opening for open or second part of flip
    if (Side::Close != signal.side) {
        const auto default_pos_size_opt = UnsignedVolume::from(m_pos_currency_amount / signal.price);
        if (!default_pos_size_opt.has_value()) {
            std::cout << "ERROR can't get proper default position size" << std::endl;
            return;
        }
        const bool success = open_position(signal.price,
                                           SignedVolume(
                                                   default_pos_size_opt.value(),
                                                   signal.side),
                                           signal.timestamp);
        if (!success) {
            std::cout << "ERROR Failed to open position" << std::endl;
            return;
        }
    }

    m_last_signal = signal;
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

void StrategyInstance::invoke(const ResponseEventVariant & var)
{
    if (const auto * r = std::get_if<MDPriceEvent>(&var); r) {
        const MDPriceEvent & response = *r;
        on_price_received(response.ts_and_price.first, response.ts_and_price.second);
        return;
    }
    if (const auto * r = std::get_if<HistoricalMDPackEvent>(&var); r) {
        const HistoricalMDPackEvent & response = *r;
        for (const auto & [ts, ohlc] : response.ts_and_price_pack) {
            on_price_received(ts, ohlc);
        }
        return;
    }
    if (const auto * r = std::get_if<OrderAcceptedEvent>(&var); r) {
        return;
    }
    if (const auto * r = std::get_if<OrderRejectedEvent>(&var); r) {
        std::cout << "Got order rejected" << std::endl;
        return;
    }
    if (const auto * r = std::get_if<TradeEvent>(&var); r) {
        const auto res = m_position_manager.on_trade_received(r->trade);
        if (res.has_value()) {
            process_position_result(res.value(), r->trade.ts);
        }
        m_strategy_result.update([&](StrategyResult & res) {
            res.trades_count++;
        });
        return;
    }
    std::cout << "ERROR: Unhandled MDResponseEvent" << std::endl;
}

bool StrategyInstance::open_position(double price, SignedVolume target_absolute_volume, std::chrono::milliseconds ts)
{
    const std::optional<SignedVolume> adjusted_target_volume_opt = m_symbol.get_qty_floored(target_absolute_volume);
    if (!adjusted_target_volume_opt.has_value()) {
        std::cout
                << "ERROR can't get proper volume on open_or_move, target_absolute_volume = "
                << target_absolute_volume.value()
                << ", price_step: "
                << m_symbol.lot_size_filter.qty_step << std::endl;
        return false;
    }
    const SignedVolume adjusted_target_volume = adjusted_target_volume_opt.value();

    if (0. == adjusted_target_volume.value()) {
        std::cout << "ERROR: closing on open_or_move" << std::endl;
        return false;
    }

    if (m_position_manager.opened() != nullptr) {
        std::cout << "ERROR: opening already opened position" << std::endl;
        return false;
    }

    const auto order = MarketOrder{
            m_symbol.symbol_name,
            price,
            adjusted_target_volume,
            ts};

    // const auto trades_opt = m_tr_gateway.send_order_sync(order);
    OrderRequestEvent or_event(
            order,
            m_event_loop,
            m_event_loop,
            m_event_loop);
    m_tr_gateway.push_order_request(or_event);
    return true;
}

bool StrategyInstance::close_position(double price, std::chrono::milliseconds ts)
{
    const auto * pos_ptr = m_position_manager.opened();
    if (pos_ptr == nullptr) {
        return false;
    }
    const auto & pos = *pos_ptr;

    const auto order = [&]() {
        const auto [vol, side] = pos.absolute_volume().as_unsigned_and_side();
        const auto volume = SignedVolume(vol, side == Side::Buy ? Side::Sell : Side::Buy);
        return MarketOrder{
                m_symbol.symbol_name,
                price,
                volume,
                ts};
    }();

    OrderRequestEvent or_event(
            order,
            m_event_loop,
            m_event_loop,
            m_event_loop);
    m_tr_gateway.push_order_request(or_event);
    return true;
}
