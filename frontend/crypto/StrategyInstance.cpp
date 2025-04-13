#include "StrategyInstance.h"

#include "DynamicTrailingStopLossStrategy.h"
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

#include <algorithm>
#include <chrono>
#include <future>
#include <optional>
#include <ranges>

// TODO move it to a separate file
class ExitStrategyFactory
{
public:
    static std::optional<std::shared_ptr<IExitStrategy>> build_exit_strategy(
            const std::string & strategy_name,
            const JsonStrategyConfig & config,
            const Symbol & symbol,
            ITradingGateway & gateway)
    {
        if (strategy_name == "TpslExit") {
            std::shared_ptr<IExitStrategy> res = std::make_shared<TpslExitStrategy>(symbol, config, gateway);
            return res;
        }
        if (strategy_name == "TrailingStop") {
            std::shared_ptr<IExitStrategy> res = std::make_shared<TrailigStopLossStrategy>(symbol, config, gateway);
            return res;
        }
        if (strategy_name == "DynamicTrailingStop") {
            std::shared_ptr<IExitStrategy> res = std::make_shared<DynamicTrailingStopLossStrategy>(symbol, config, gateway);
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
    , m_candle_builder{strategy_ptr->timeframe().value_or(std::chrono::minutes{5})}
    , m_md_gateway(md_gateway)
    , m_tr_gateway(tr_gateway)
    , m_strategy(strategy_ptr)
    , m_symbol(symbol)
    , m_position_manager(symbol)
    , m_exit_strategy(nullptr)
    , m_historical_md_request(historical_md_request)
{
    // TODO build exit strategy outside
    const auto exit_strategy_opt = ExitStrategyFactory::build_exit_strategy(
            exit_strategy_name,
            exit_strategy_config,
            symbol,
            tr_gateway);
    if (!exit_strategy_opt) {
        Logger::log<LogLevel::Error>("Can't build exit strategy");
        m_status.push(WorkStatus::Panic);
        return;
    }
    m_exit_strategy = *exit_strategy_opt;

    m_status.push(WorkStatus::Stopped);

    m_event_loop.subscribe(m_md_gateway.historical_prices_channel());
    m_event_loop.subscribe(m_md_gateway.historical_lowmem_channel());
    m_event_loop.subscribe(m_md_gateway.live_prices_channel());

    m_event_loop.subscribe(m_tr_gateway.order_response_channel());
    m_event_loop.subscribe(m_tr_gateway.trade_channel());
    m_event_loop.subscribe(m_tr_gateway.tpsl_response_channel());
    m_event_loop.subscribe(m_tr_gateway.tpsl_updated_channel());
    m_event_loop.subscribe(m_tr_gateway.trailing_stop_response_channel());
    m_event_loop.subscribe(m_tr_gateway.trailing_stop_update_channel());

    m_strategy_result.update([&](StrategyResult & res) {
        res.position_currency_amount = m_pos_currency_amount;
    });

    m_event_loop.subscribe(m_md_gateway.status_channel(),
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

    register_invokers();
}

StrategyInstance::~StrategyInstance()
{
    Logger::log<LogLevel::Status>("StrategyInstance destructor");
}

void StrategyInstance::run_async()
{
    if (m_historical_md_request.has_value()) {
        m_status.push(WorkStatus::Backtesting);

        HistoricalMDRequest historical_request(
                m_symbol,
                m_historical_md_request.value());
        m_pending_requests.emplace(historical_request.guid);
        m_md_gateway.push_async_request(std::move(historical_request));
    }
    else {
        m_status.push(WorkStatus::Live);
        LiveMDRequest live_request(m_symbol);
        m_live_md_requests.emplace(live_request.guid);
        m_md_gateway.push_async_request(std::move(live_request));
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
    m_event_loop.push_event(StrategyStopRequest{});
}

std::future<void> StrategyInstance::finish_future()
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
    m_positions_channel.push(ts, new_result);

    m_strategy_result.update([&](StrategyResult & res) {
        res.final_profit += new_result.pnl_with_fee;
        res.fees_paid += new_result.fees_paid;

        if (new_result.pnl_with_fee > 0.) {
            res.profit_positions_cnt++;
        }
        else {
            res.loss_positions_cnt++;
        }

        if (!res.best_profit_trade.has_value() || res.best_profit_trade < new_result.pnl_with_fee) {
            res.best_profit_trade = new_result.pnl_with_fee;
        }
        if (!res.worst_loss_trade.has_value() || res.worst_loss_trade > new_result.pnl_with_fee) {
            res.worst_loss_trade = new_result.pnl_with_fee;
        }

        if (new_result.pnl_with_fee > 0.) {
            if (res.longest_profit_trade_time < new_result.opened_time()) {
                res.longest_profit_trade_time =
                        std::chrono::duration_cast<std::chrono::seconds>(new_result.opened_time());
            }
            res.total_time_in_profit_pos += std::chrono::duration_cast<std::chrono::seconds>(new_result.opened_time());
        }
        if (new_result.pnl_with_fee < 0.) {
            if (res.longest_loss_trade_time < new_result.opened_time()) {
                res.longest_loss_trade_time =
                        std::chrono::duration_cast<std::chrono::seconds>(new_result.opened_time());
            }
            res.total_time_in_loss_pos += std::chrono::duration_cast<std::chrono::seconds>(new_result.opened_time());
        }

        res.avg_profit_pos_time = static_cast<double>(res.total_time_in_profit_pos.count()) / static_cast<double>(res.profit_positions_cnt);
        res.avg_loss_pos_time = static_cast<double>(res.total_time_in_loss_pos.count()) / static_cast<double>(res.loss_positions_cnt);

        res.max_depo = std::max(res.max_depo, res.final_profit);
        res.min_depo = std::min(res.min_depo, res.final_profit);
    });

    m_depo_channel.push(ts, m_strategy_result.get().final_profit);

    const std::list<std::pair<std::chrono::milliseconds, double>> depo_series_list = m_depo_channel.data_copy();
    const std::vector<std::pair<std::chrono::milliseconds, double>> depo_series{
            depo_series_list.begin(),
            depo_series_list.end()};
    m_strategy_result.update([&](StrategyResult & res) {
        res.set_trend_info(depo_series);
    });
}

void StrategyInstance::set_channel_capacity(std::optional<std::chrono::milliseconds> capacity)
{
    trade_channel().set_capacity(capacity);
    strategy_internal_data_channel().set_capacity(capacity);
    candle_channel().set_capacity(capacity);
    // depo_channel().set_capacity(capacity);
    tpsl_channel().set_capacity(capacity);
    trailing_stop_channel().set_capacity(capacity);
    price_channel().set_capacity(capacity);
}

EventTimeseriesChannel<ProfitPriceLevels> & StrategyInstance::price_levels_channel()
{
    return m_price_levels_channel;
}

EventTimeseriesChannel<Trade> & StrategyInstance::trade_channel()
{
    return m_trade_channel;
}

EventTimeseriesChannel<std::tuple<std::string, std::string, double>> & StrategyInstance::strategy_internal_data_channel()
{
    return m_strategy->strategy_internal_data_channel();
}

EventTimeseriesChannel<double> & StrategyInstance::price_channel()
{
    return m_price_channel;
}

EventTimeseriesChannel<Candle> & StrategyInstance::candle_channel()
{
    return m_candle_channel;
}

EventTimeseriesChannel<double> & StrategyInstance::depo_channel()
{
    return m_depo_channel;
}

EventTimeseriesChannel<PositionResult> & StrategyInstance::positions_channel()
{
    return m_positions_channel;
}

EventObjectChannel<StrategyResult> & StrategyInstance::strategy_result_channel()
{
    return m_strategy_result;
}

EventObjectChannel<WorkStatus> & StrategyInstance::status_channel()
{
    return m_status;
}

EventTimeseriesChannel<Tpsl> & StrategyInstance::tpsl_channel()
{
    return m_exit_strategy->tpsl_channel();
}

EventTimeseriesChannel<StopLoss> & StrategyInstance::trailing_stop_channel()
{
    return m_exit_strategy->trailing_stop_channel();
}

void StrategyInstance::register_invokers()
{
    m_subscriptions.push_back(m_event_loop.invoker().register_invoker<HistoricalMDGeneratorEvent>([&](const auto & response) {
        // can get history MD ev from an other strategy because of fan-out channels. // TODO
        if (m_stop_request_handled) { // TODO handle with guid
            return;
        }
        handle_event(response);
        after_every_event();
    }));

    m_subscriptions.push_back(
            m_event_loop.invoker().register_invoker<HistoricalMDGeneratorLowMemEvent>(
                    [&](const auto & response) {
                        handle_event(response);
                        after_every_event();
                    }));
    m_subscriptions.push_back(
            m_event_loop.invoker().register_invoker<HistoricalMDPriceEvent>(
                    [&](const auto & response) {
                        handle_event(response);
                        after_every_event();
                    }));
    m_subscriptions.push_back(
            m_event_loop.invoker().register_invoker<MDPriceEvent>(
                    [&](const auto & response) {
                        handle_event(response);
                        after_every_event();
                    }));
    m_subscriptions.push_back(
            m_event_loop.invoker().register_invoker<OrderResponseEvent>(
                    [&](const auto & response) {
                        handle_event(response);
                        after_every_event();
                    }));
    m_subscriptions.push_back(
            m_event_loop.invoker().register_invoker<TradeEvent>(
                    [&](const auto & r) {
                        handle_event(r);
                        after_every_event();
                    }));
    m_subscriptions.push_back(
            m_event_loop.invoker().register_invoker<TpslResponseEvent>(
                    [&](const auto & response) {
                        handle_event(response);
                        after_every_event();
                    }));
    m_subscriptions.push_back(
            m_event_loop.invoker().register_invoker<TpslUpdatedEvent>(
                    [&](const auto & response) {
                        handle_event(response);
                        after_every_event();
                    }));
    m_subscriptions.push_back(
            m_event_loop.invoker().register_invoker<TrailingStopLossResponseEvent>(
                    [&](const auto & response) {
                        handle_event(response);
                        after_every_event();
                    }));
    m_subscriptions.push_back(
            m_event_loop.invoker().register_invoker<TrailingStopLossUpdatedEvent>(
                    [&](const auto & response) {
                        handle_event(response);
                        after_every_event();
                    }));
    m_subscriptions.push_back(
            m_event_loop.invoker().register_invoker<LambdaEvent>(
                    [&](const auto & response) {
                        handle_event(response);
                        after_every_event();
                    }));
    m_subscriptions.push_back(
            m_event_loop.invoker().register_invoker<StrategyStopRequest>(
                    [&](const auto & response) {
                        handle_event(response);
                        after_every_event();
                    }));
}

// TODO maybe make it more elegant?
void StrategyInstance::after_every_event()
{
    if (ready_to_finish()) {
        m_status.push(m_status_on_stop);
        m_depo_channel.push(m_last_ts_and_price.first, m_strategy_result.get().final_profit);
        // TODO unsub from TRGW?
    }
    finish_if_needed_and_ready();
}

void StrategyInstance::handle_event(const HistoricalMDGeneratorEvent & response)
{
    const size_t erased_cnt = m_pending_requests.erase(response.request_guid());
    if (erased_cnt == 0) {
        Logger::logf<LogLevel::Debug>("unsolicited HistoricalMDPackEvent: {}, this->guid: {}", response.request_guid(), m_strategy_guid);
        return;
    }

    if (!response.has_data()) {
        Logger::logf<LogLevel::Error>("no data in HistoricalMDPackEvent: {}", response.request_guid());
        stop_async(true);
        return;
    }

    m_historical_md_generator = response;
    const auto ev_opt = m_historical_md_generator->get_next();
    if (!ev_opt.has_value()) {
        Logger::logf<LogLevel::Error>("no event in HistoricalMDPackEvent: {}", response.request_guid());
        return;
    }

    m_backtest_in_progress = true;
    m_event_loop.push_event(ev_opt.value());
}

void StrategyInstance::handle_event(const HistoricalMDGeneratorLowMemEvent & response)
{
    const size_t erased_cnt = m_pending_requests.erase(response.request_guid());
    if (erased_cnt == 0) {
        Logger::logf<LogLevel::Debug>("unsolicited HistoricalMDPackEvent: {}, this->guid: {}", response.request_guid(), m_strategy_guid);
        return;
    }

    m_historical_md_lowmem_generator = response;
    const auto ev_opt = m_historical_md_lowmem_generator->get_next();
    if (!ev_opt.has_value()) {
        Logger::logf<LogLevel::Error>("no event in HistoricalMDGeneratorLowMemEvent: {}", response.request_guid());
        return;
    }

    m_backtest_in_progress = true;
    m_event_loop.push_event(ev_opt.value());
}

void StrategyInstance::handle_event(const HistoricalMDPriceEvent & response)
{
    handle_event(static_cast<const MDPriceEvent &>(response));
    auto ev_opt = response.lowmem ? m_historical_md_lowmem_generator->get_next() : m_historical_md_generator->get_next();
    if (!ev_opt.has_value()) {
        m_event_loop.push_event(StrategyStopRequest{});
        if (response.lowmem) {
            m_historical_md_lowmem_generator.reset();
        }
        else {
            m_historical_md_generator.reset();
        }
        return;
    }
    const auto & ev = ev_opt.value();
    m_event_loop.push_event(ev);
}

void StrategyInstance::handle_event(const MDPriceEvent & response)
{
    const auto & public_trade = response.public_trade;
    m_last_ts_and_price = {public_trade.ts(), public_trade.price()};
    m_price_channel.push(public_trade.ts(), public_trade.price());
    if (!first_price_received) {
        m_depo_channel.push(public_trade.ts(), 0.); // TODO move it to c-tor?
        first_price_received = true;
    }

    const auto candles = m_candle_builder.push_trade(public_trade.price(), public_trade.volume(), public_trade.ts());
    std::optional<Signal> signal_opt;
    for (const auto & candle : candles) {
        auto s = m_strategy->push_candle(candle);
        if (s.has_value()) {
            signal_opt = s.value();
        }
        m_candle_channel.push(candle.ts(), candle);
    }

    if (!signal_opt) {
        signal_opt = m_strategy->push_price({public_trade.ts(), public_trade.price()});
    }
    if (m_position_manager.opened() == nullptr && m_pending_orders.empty()) {
        if (signal_opt.has_value()) {
            on_signal(signal_opt.value());
        }
    }

    if (const auto err = m_exit_strategy->on_price_changed({public_trade.ts(), public_trade.price()})) {
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
    OrderRequestEvent or_event{order};
    m_pending_orders.emplace(order.guid(), order);
    Logger::logf<LogLevel::Debug>("Re-sending order with guid: {}", order.guid());
    m_tr_gateway.push_order_request(or_event);
}

void StrategyInstance::handle_event(const TradeEvent & response)
{
    const auto & trade = response.trade;
    Logger::logf<LogLevel::Debug>("Trade received: {}", trade);
    const auto res = m_position_manager.on_trade_received(response.trade);

    m_trade_channel.push(trade.ts(), trade);

    if (res.has_value()) {
        process_position_result(res.value(), trade.ts());
    }
    m_strategy_result.update([&](StrategyResult & res) {
        res.trades_count++;
        res.set_last_trade_date(trade.ts());
    });

    const auto pos = [&]() -> std::optional<OpenedPosition> {
        if (m_position_manager.opened() == nullptr) {
            return std::nullopt;
        }
        else {
            return *m_position_manager.opened();
        }
    }();

    if (pos.has_value()) {
        m_price_levels_channel.push(trade.ts(), pos->price_levels());
        m_exit_strategy->push_price_level(pos->price_levels());
    }

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
    m_historical_md_generator.reset();
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

    OrderRequestEvent or_event{order};
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

    OrderRequestEvent or_event{order};
    m_pending_orders.emplace(order.guid(), order);
    m_tr_gateway.push_order_request(or_event);
    return true;
}

bool StrategyInstance::ready_to_finish() const
{
    const bool pos_closed = m_position_manager.opened() == nullptr;
    const bool got_active_requests = m_pending_requests.empty() && m_live_md_requests.empty();
    const bool historical_md_finished = !m_historical_md_generator.has_value();
    const bool res = pos_closed && got_active_requests && !m_backtest_in_progress && historical_md_finished;
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
