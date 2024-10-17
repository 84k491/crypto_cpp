#include "StrategyInstance.h"

#include "EventLoop.h"
#include "Events.h"
#include "ITradingGateway.h"
#include "Logger.h"
#include "ScopeExit.h"
#include "Signal.h"
#include "TpslExitStrategy.h"
#include "TrailingStopStrategy.h"
#include "Volume.h"
#include "WorkStatus.h"

#include <chrono>
#include <future>
#include <optional>

// TODO move it to a separate file
class ExitStrategyFactory
{
public:
    static std::optional<std::shared_ptr<IExitStrategy>> build_exit_strategy(
            const std::string & strategy_name,
            const JsonStrategyConfig & config,
            const Symbol & symbol,
            EventLoopHolder<STRATEGY_EVENTS> & event_loop,
            ITradingGateway & gateway)
    {
        if (strategy_name == "TpslExit") {
            std::shared_ptr<IExitStrategy> res = std::make_shared<TpslExitStrategy>(symbol, config, event_loop, gateway);
            return res;
        }
        if (strategy_name == "TrailingStop") {
            std::shared_ptr<IExitStrategy> res = std::make_shared<TrailigStopLossStrategy>(symbol, config, event_loop, gateway);
            return res;
        }
        Logger::logf<LogLevel::Error>("Unknown exit strategy name: {}", strategy_name);
        return {};
    }
};

StrategyInstance::StrategyInstance(
        const Symbol & symbol,
        const std::optional<HistoricalMDRequestData> & historical_md_request,
        const std::shared_ptr<IStrategy> & strategy_ptr,
        const std::string & exit_strategy_name,
        const JsonStrategyConfig & exit_strategy_config,
        IMarketDataGateway & md_gateway,
        ITradingGateway & tr_gateway)
    : m_strategy_guid(xg::newGuid())
    , m_md_gateway(md_gateway)
    , m_tr_gateway(tr_gateway)
    , m_strategy(strategy_ptr)
    , m_symbol(symbol)
    , m_position_manager(symbol)
    , m_exit_strategy(nullptr)
    , m_historical_md_request(historical_md_request)
    , m_event_loop(*this)
{
    const auto exit_strategy_opt = ExitStrategyFactory::build_exit_strategy(
            exit_strategy_name,
            exit_strategy_config,
            symbol,
            m_event_loop,
            tr_gateway);
    if (!exit_strategy_opt) {
        Logger::log<LogLevel::Error>("Can't build exit strategy");
        m_status.push(WorkStatus::Panic);
        return;
    }
    m_exit_strategy = *exit_strategy_opt;

    m_status.push(WorkStatus::Stopped);

    m_subscriptions.push_back(m_md_gateway.historical_prices_publisher().subscribe(m_event_loop.sptr()));
    m_subscriptions.push_back(m_md_gateway.live_prices_publisher().subscribe(m_event_loop.sptr()));
    m_subscriptions.push_back(m_tr_gateway.order_response_publisher().subscribe(m_event_loop.sptr()));

    m_tr_gateway.register_consumers(
            m_strategy_guid,
            symbol,
            TradingGatewayConsumers{
                    // TODO remove this class ?
                    .trade_consumer = *m_event_loop,
                    .tpsl_response_consumer = *m_event_loop,
                    .tpsl_update_consumer = *m_event_loop,
                    .trailing_stop_update_consumer = *m_event_loop,
            });
    m_strategy_result.update([&](StrategyResult & res) {
        res.position_currency_amount = m_pos_currency_amount;
    });

    m_gw_status_sub = m_md_gateway.status_publisher().subscribe(
            m_event_loop.sptr(),
            [this](const WorkStatus & status) {
                if (status == WorkStatus::Stopped || status == WorkStatus::Panic) {
                    if (m_position_manager.opened() != nullptr) {
                        const bool success = close_position(m_last_ts_and_price.second, m_last_ts_and_price.first);
                        if (!success) {
                            std::cout << "ERROR: can't close a position on stop/crash" << std::endl;
                        }
                    }
                }
            });
}

StrategyInstance::~StrategyInstance()
{
    Logger::log<LogLevel::Status>("StrategyInstance destructor");
    m_tr_gateway.unregister_consumers(m_strategy_guid);
}

void StrategyInstance::run_async()
{
    if (m_historical_md_request.has_value()) {
        m_status.push(WorkStatus::Backtesting);

        HistoricalMDRequest historical_request(
                m_symbol,
                m_historical_md_request.value());
        m_md_gateway.push_async_request(std::move(historical_request));
        m_pending_requests.emplace(historical_request.guid);
    }
    else {
        m_status.push(WorkStatus::Live);
        LiveMDRequest live_request(m_symbol);
        m_md_gateway.push_async_request(std::move(live_request));
        m_live_md_requests.emplace(live_request.guid);
    }
}

void StrategyInstance::stop_async(bool panic)
{
    Logger::log<LogLevel::Status>("stop_async");
    for (const auto & req : m_live_md_requests) {
        m_md_gateway.unsubscribe_from_live(req);
    }
    if (panic) {
        m_status_on_stop = WorkStatus::Panic;
    }
    static_cast<IEventConsumer<StrategyStopRequest> &>(*m_event_loop).push(StrategyStopRequest{});
}

std::future<void> StrategyInstance::wait_for_finish()
{
    m_finish_promise = std::promise<void>();
    if (ready_to_finish()) {
        m_finish_promise->set_value();
    }
    return m_finish_promise->get_future();
}

void StrategyInstance::on_signal(const Signal & signal)
{
    // closing if close or flip
    if ((m_position_manager.opened() != nullptr && signal.side != m_position_manager.opened()->side())) {
        if (m_position_manager.opened() != nullptr) {
            const bool success = close_position(signal.price, signal.timestamp);
            if (!success) {
                Logger::log<LogLevel::Error>("Failed to close a position");
            }
        }
    }

    // opening for open or second part of flip
    const auto default_pos_size_opt = UnsignedVolume::from(m_pos_currency_amount / signal.price);
    if (!default_pos_size_opt.has_value()) {
        Logger::log<LogLevel::Error>("can't get proper default position size");
        return;
    }
    const bool success = open_position(signal.price,
                                       SignedVolume(
                                               default_pos_size_opt.value(),
                                               signal.side),
                                       signal.timestamp);
    if (!success) {
        Logger::log<LogLevel::Error>("Failed to open position");
        return;
    }
}

void StrategyInstance::process_position_result(const PositionResult & new_result, std::chrono::milliseconds ts)
{
    m_strategy_result.update([&](StrategyResult & res) {
        res.final_profit += new_result.pnl_with_fee;
        res.fees_paid += new_result.fees_paid;
    });
    m_depo_publisher.push(ts, m_strategy_result.get().final_profit);
    m_strategy_result.update([&](StrategyResult & res) {
        if (new_result.pnl_with_fee > 0.) {
            res.profit_positions_cnt++;
        }
        else {
            res.loss_positions_cnt++;
        }
    });
    if (const auto best_profit = m_strategy_result.get().best_profit_trade;
        !best_profit.has_value() || best_profit < new_result.pnl_with_fee) {
        m_strategy_result.update([&](StrategyResult & res) {
            res.best_profit_trade = new_result.pnl_with_fee;
            res.longest_profit_trade_time =
                    std::chrono::duration_cast<std::chrono::seconds>(new_result.opened_time);
        });
    }
    if (const auto worst_loss = m_strategy_result.get().worst_loss_trade;
        !worst_loss.has_value() || worst_loss > new_result.pnl_with_fee) {
        m_strategy_result.update([&](StrategyResult & res) {
            res.worst_loss_trade = new_result.pnl_with_fee;
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

EventTimeseriesPublisher<Signal> & StrategyInstance::signals_publisher()
{
    return m_signal_publisher;
}

EventTimeseriesPublisher<std::tuple<std::string, std::string, double>> & StrategyInstance::strategy_internal_data_publisher()
{
    return m_strategy->strategy_internal_data_publisher();
}

EventTimeseriesPublisher<OHLC> & StrategyInstance::klines_publisher()
{
    return m_klines_publisher;
}

EventTimeseriesPublisher<double> & StrategyInstance::depo_publisher()
{
    return m_depo_publisher;
}

EventObjectPublisher<StrategyResult> & StrategyInstance::strategy_result_publisher()
{
    return m_strategy_result;
}

EventObjectPublisher<WorkStatus> & StrategyInstance::status_publisher()
{
    return m_status;
}

EventTimeseriesPublisher<Tpsl> & StrategyInstance::tpsl_publisher()
{
    return m_exit_strategy->tpsl_publisher();
}

EventTimeseriesPublisher<StopLoss> & StrategyInstance::trailing_stop_publisher()
{
    return m_exit_strategy->trailing_stop_publisher();
}

void StrategyInstance::invoke(const std::variant<STRATEGY_EVENTS> & var)
{
    std::visit(
            VariantMatcher{
                    [&](const HistoricalMDPackEvent & response) {
                        handle_event(response);
                    },
                    [&](const MDPriceEvent & response) {
                        handle_event(response);
                    },
                    [&](const OrderResponseEvent & response) {
                        handle_event(response);
                    },
                    [&](const TradeEvent & r) {
                        handle_event(r);
                    },
                    [&](const TpslResponseEvent & response) {
                        handle_event(response);
                    },
                    [&](const TpslUpdatedEvent & response) {
                        handle_event(response);
                    },
                    [&](const TrailingStopLossResponseEvent & response) {
                        handle_event(response);
                    },
                    [&](const TrailingStopLossUpdatedEvent & response) {
                        handle_event(response);
                    },
                    [&](const LambdaEvent & response) {
                        handle_event(response);
                    },
                    [&](const StrategyStopRequest & response) {
                        handle_event(response);
                    }},
            var);

    if (ready_to_finish()) {
        m_status.push(m_status_on_stop);
        m_depo_publisher.push(m_last_ts_and_price.first, m_strategy_result.get().final_profit);
        m_tr_gateway.unregister_consumers(m_strategy_guid);
    }
    finish_if_needed_and_ready();
}

void StrategyInstance::handle_event(const HistoricalMDPackEvent & response)
{
    if (response.ts_and_price_pack == nullptr) {
        Logger::log<LogLevel::Error>("response.ts_and_price_pack == nullptr");
        stop_async(true);
        return;
    }

    for (const auto & [ts, ohlc] : *response.ts_and_price_pack) {
        MDPriceEvent ev;
        ev.ts_and_price = {ts, ohlc};
        static_cast<IEventConsumer<MDPriceEvent> &>(*m_event_loop).push(ev);
    }

    m_backtest_in_progress = true;
    static_cast<IEventConsumer<StrategyStopRequest> &>(*m_event_loop).push(StrategyStopRequest{});
    const size_t erased_cnt = m_pending_requests.erase(response.request_guid);
    if (erased_cnt == 0) {
        Logger::log<LogLevel::Error>("unsolicited HistoricalMDPackEvent");
    }
}

void StrategyInstance::handle_event(const MDPriceEvent & response)
{
    const auto & [ts, ohlc] = response.ts_and_price;
    m_last_ts_and_price = {ts, ohlc.close};
    m_klines_publisher.push(ts, ohlc);
    if (!first_price_received) {
        m_depo_publisher.push(ts, 0.);
        first_price_received = true;
    }

    const auto signal = m_strategy->push_price({ts, ohlc.close});
    if (m_position_manager.opened() == nullptr && m_pending_orders.empty()) {
        if (signal.has_value()) {
            on_signal(signal.value());
        }
    }

    if (const auto err = m_exit_strategy->on_price_changed({ts, ohlc.close})) {
        Logger::logf<LogLevel::Error>("Exit strategy error on price update: {}", err.value());
    }
}

void StrategyInstance::handle_event(const OrderResponseEvent & response)
{
    if (response.reject_reason.has_value() && !response.retry) {
        Logger::logf<LogLevel::Warning>("OrderRejected: {}", response.reject_reason.value());
        stop_async(true);
    }

    const auto it = m_pending_orders.find(response.request_guid);
    if (it == m_pending_orders.end()) {
        Logger::log<LogLevel::Error>("unsolicited OrderResponseEvent");
        return;
    }
    ScopeExit se([&]() { m_pending_orders.erase(it); });

    // TODO trades are counted on every order send retry. Fix it
    if (!response.retry) {
        return;
    }

    auto [guid, order] = *it; // copy
    order.regenerate_guid();
    OrderRequestEvent or_event(
            order,
            m_event_loop.sptr(),
            m_event_loop.sptr());
    m_pending_orders.emplace(order.guid(), order);
    Logger::logf<LogLevel::Debug>("Re-sending order with guid: {}", order.guid());
    m_tr_gateway.push_order_request(or_event);
}

void StrategyInstance::handle_event(const TradeEvent & response)
{
    const auto res = m_position_manager.on_trade_received(response.trade);

    const auto & trade = response.trade;
    m_signal_publisher.push(trade.ts(), Signal{trade.side(), trade.ts(), trade.price()});

    if (res.has_value()) {
        process_position_result(res.value(), trade.ts());
    }
    m_strategy_result.update([&](StrategyResult & res) {
        res.trades_count++;
    });

    const auto pos = [&]() -> std::optional<OpenedPosition> {
        if (m_position_manager.opened() == nullptr) {
            return std::nullopt;
        }
        else {
            return *m_position_manager.opened();
        }
    }();

    if (const auto err = m_exit_strategy->on_trade(pos, trade); err.has_value()) {
        // stop_async(true);
        Logger::log<LogLevel::Error>(std::string{err.value()});
    }
}

void StrategyInstance::handle_event(const TpslResponseEvent & response)
{
    // TODO make an Error class
    const auto err_pair_opt = m_exit_strategy->handle_event(response);

    if (err_pair_opt.has_value()) {
        const auto & [err, do_panic] = err_pair_opt.value();
        Logger::log<LogLevel::Error>(std::string(err));
        stop_async(do_panic);
    }
}

void StrategyInstance::handle_event(const TpslUpdatedEvent & response)
{
    const auto err_pair_opt = m_exit_strategy->handle_event(response);
    if (err_pair_opt.has_value()) {
        const auto & [err, do_panic] = err_pair_opt.value();
        Logger::log<LogLevel::Error>(std::string(err));
        stop_async(do_panic);
    }
}

void StrategyInstance::handle_event(const StrategyStopRequest &)
{
    Logger::log<LogLevel::Status>("StrategyStopRequest");
    if (m_position_manager.opened() != nullptr) {
        const bool success = close_position(m_last_ts_and_price.second, m_last_ts_and_price.first);
        if (!success) {
            Logger::log<LogLevel::Error>("ERROR: can't close a position on stop/panic");
        }
    }

    m_live_md_requests.clear();

    m_backtest_in_progress = false;
    m_stop_request_handled = true;
}

void StrategyInstance::handle_event(const LambdaEvent & response)
{
    response.func();
}

bool StrategyInstance::open_position(double price, SignedVolume target_absolute_volume, std::chrono::milliseconds ts)
{
    const std::optional<SignedVolume> adjusted_target_volume_opt = m_symbol.get_qty_floored(target_absolute_volume);
    if (!adjusted_target_volume_opt.has_value()) {
        Logger::logf<LogLevel::Error>(
                "ERROR can't get proper volume on open_or_move, target_absolute_volume = {}, qty_step = {}",
                target_absolute_volume.value(),
                m_symbol.lot_size_filter.qty_step);
        return false;
    }
    const SignedVolume adjusted_target_volume = adjusted_target_volume_opt.value();

    if (0. == adjusted_target_volume.value()) {
        Logger::log<LogLevel::Error>("closing on open_or_move");
        return false;
    }

    if (m_position_manager.opened() != nullptr) {
        Logger::log<LogLevel::Error>("opening already opened position");
        return false;
    }

    const auto order = MarketOrder{
            m_symbol.symbol_name,
            price,
            adjusted_target_volume,
            ts};

    OrderRequestEvent or_event(
            order,
            m_event_loop.sptr(),
            m_event_loop.sptr());
    m_pending_orders.emplace(order.guid(), order);
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
        const auto [vol, side] = pos.opened_volume().as_unsigned_and_side();
        const auto volume = SignedVolume(vol, side.opposite());
        return MarketOrder{
                m_symbol.symbol_name,
                price,
                volume,
                ts};
    }();

    OrderRequestEvent or_event(
            order,
            m_event_loop.sptr(),
            m_event_loop.sptr());
    m_pending_orders.emplace(order.guid(), order);
    m_tr_gateway.push_order_request(or_event);
    return true;
}

bool StrategyInstance::ready_to_finish() const
{
    const bool pos_closed = m_position_manager.opened() == nullptr;
    const bool got_active_requests = m_pending_requests.empty() && m_live_md_requests.empty();
    const bool res = pos_closed && got_active_requests && !m_backtest_in_progress;
    if (res) {
        Logger::log<LogLevel::Status>("StrategyInstance is ready to finish");
    }
    return res;
}

void StrategyInstance::finish_if_needed_and_ready()
{
    if (m_finish_promise.has_value() && m_stop_request_handled) {
        if (ready_to_finish()) {
            auto & promise = m_finish_promise.value();
            promise.set_value(); // TODO double set on stop
        }
        else {
            // std::cout << "Waiting for all criterias to be satisfied for finish" << std::endl;
            return;
        }
    }
}

void StrategyInstance::handle_event(const TrailingStopLossResponseEvent & response)
{
    if (const auto err = m_exit_strategy->handle_event(response); err.has_value()) {
        const auto & [err_str, do_panic] = err.value();
        Logger::log<LogLevel::Error>(std::string(err_str));
        stop_async(do_panic);
    }
}

void StrategyInstance::handle_event(const TrailingStopLossUpdatedEvent & response)
{
    if (const auto err = m_exit_strategy->handle_event(response); err.has_value()) {
        const auto & [err_str, do_panic] = err.value();
        Logger::log<LogLevel::Error>(std::string(err_str));
        stop_async(do_panic);
    }
}
