#include "StrategyInstance.h"

#include "ConditionalOrders.h"
#include "EventBarrier.h"
#include "EventLoop.h"
#include "Events.h"
#include "ITradingGateway.h"
#include "Logger.h"
#include "ScopeExit.h"
#include "StrategyFactory.h"
#include "TpslExitStrategy.h"
#include "TrailingStopStrategy.h"
#include "Volume.h"
#include "WorkStatus.h"

#include <algorithm>
#include <chrono>
#include <future>
#include <optional>
#include <ranges>

namespace {
std::optional<std::chrono::milliseconds> get_timeframe(const JsonStrategyConfig & conf)
{
    if (conf.get().contains("timeframe_s")) {
        return std::chrono::seconds(conf.get()["timeframe_s"].get<int>());
    }
    return {};
}
} // namespace

StrategyInstance::StrategyInstance(
        const Symbol & symbol,
        const std::optional<HistoricalMDRequestData> & historical_md_request,
        const std::string & entry_strategy_name,
        const JsonStrategyConfig & entry_strategy_config,
        IMarketDataGateway & md_gateway,
        ITradingGateway & tr_gateway)
    : m_strategy_guid(xg::newGuid())
    , m_candle_builder{get_timeframe(entry_strategy_config).value_or(std::chrono::minutes{5})}
    , m_md_gateway(md_gateway)
    , m_tr_gateway(tr_gateway)
    , m_strategy_channels(
              m_price_channel,
              m_candle_channel,
              m_opened_pos_channel,
              m_trade_channel,
              m_price_levels_channel,
              m_trailing_stop_channel)
    , m_symbol(symbol)
    , m_position_manager(symbol)
    , m_historical_md_request(historical_md_request)
    , m_orders(symbol, m_event_loop, tr_gateway)
    , m_sub(m_event_loop)
{
    const auto strategy_ptr_opt = StrategyFactory::i().build_strategy(
            entry_strategy_name,
            entry_strategy_config,
            m_event_loop,
            m_strategy_channels,
            m_orders);

    if (!strategy_ptr_opt.has_value() || !strategy_ptr_opt.value() || !strategy_ptr_opt.value()->is_valid()) {
        Logger::log<LogLevel::Error>("Failed to build entry strategy");
        m_status.push(WorkStatus::Panic);
        return;
    }
    m_strategy = strategy_ptr_opt.value();

    m_status.push(WorkStatus::Stopped);

    m_sub.subscribe(
            m_strategy->error_channel(),
            [this](const std::pair<std::string, bool> & err) {
                const auto & [msg, do_panic] = err;
                if (do_panic) {
                    Logger::logf<LogLevel::Error>("Panic: {}", msg);
                }
                else {
                    Logger::logf<LogLevel::Warning>("Stopping strategy {} on error: {}", m_strategy_guid, msg);
                }
                stop_async(do_panic);
            },
            Priority::High);

    m_sub.subscribe(
            m_md_gateway.historical_prices_channel(),
            [this](const HistoricalMDGeneratorEvent & e) {
                handle_event_generic(e);
            },
            Priority::Low);
    m_sub.subscribe(
            m_md_gateway.live_prices_channel(),
            [this](const MDPriceEvent & e) {
                handle_event_generic(e);
            },
            Priority::Low);
    m_sub.subscribe(
            m_historical_md_channel,
            [this](const HistoricalMDPriceEvent & e) {
                handle_event_generic(e);
            },
            Priority::Low);

    m_sub.subscribe(
            m_start_ev_channel,
            [this](const StrategyStartRequest & e) {
                handle_event_generic(e);
            });
    m_sub.subscribe(
            m_stop_ev_channel,
            [this](const StrategyStopRequest & e) {
                handle_event_generic(e);
            });

    m_sub.subscribe(
            m_tr_gateway.trade_channel(),
            [this](const TradeEvent & e) {
                handle_event_generic(e);
            });

    m_strategy_result.update([&](StrategyResult & res) {
        res.position_currency_amount = m_pos_currency_amount;
    });

    if (!historical_md_request.has_value()) {
        m_sub.subscribe(m_md_gateway.status_channel(),
                        [this](const WorkStatus & status) {
                            if (status == WorkStatus::Stopped || status == WorkStatus::Panic) {
                                if (m_position_manager.opened() != nullptr) {
                                    close_position(
                                            m_last_ts_and_price.second,
                                            m_last_ts_and_price.first);
                                }
                            }
                        });
    }
}

StrategyInstance::~StrategyInstance()
{
    Logger::log<LogLevel::Status>("StrategyInstance destructor");
}

void StrategyInstance::run_async()
{
    m_start_ev_channel.push({});
}

void StrategyInstance::stop_async(bool panic)
{
    Logger::log<LogLevel::Status>("stop_async");
    for (const auto & req : m_live_md_requests) {
        m_md_gateway.unsubscribe_from_live(req);
    }
    if (panic) {
        m_status.push(WorkStatus::Panic);
        m_status_on_stop = WorkStatus::Panic;
    }
    m_stop_ev_channel.push({});
}

std::future<void> StrategyInstance::finish_future()
{
    return m_finish_promise.get_future();
}

void StrategyInstance::process_position_result(const PositionResult & new_result, std::chrono::milliseconds ts)
{
    m_positions_channel.push(ts, new_result);

    m_strategy_result.update([&](StrategyResult & res) {
        const auto old = m_previous_profit.value_or(0.);
        res.final_profit -= old;
        res.final_profit += new_result.pnl_with_fee;
        m_previous_profit = new_result.pnl_with_fee;

        if (new_result.close_ts != std::chrono::milliseconds{}) {
            m_previous_profit = {};
        }

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

        const auto opened_time_opt = new_result.opened_time();
        if (opened_time_opt.has_value()) {
            const auto & opened_time = opened_time_opt.value();
            if (new_result.pnl_with_fee > 0.) {
                if (res.longest_profit_trade_time < opened_time) {
                    res.longest_profit_trade_time =
                            std::chrono::duration_cast<std::chrono::seconds>(opened_time);
                }
                res.total_time_in_profit_pos += std::chrono::duration_cast<std::chrono::seconds>(opened_time);
            }
            if (new_result.pnl_with_fee < 0.) {
                if (res.longest_loss_trade_time < opened_time) {
                    res.longest_loss_trade_time =
                            std::chrono::duration_cast<std::chrono::seconds>(opened_time);
                }
                res.total_time_in_loss_pos += std::chrono::duration_cast<std::chrono::seconds>(opened_time);
            }
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
    // depo_channel().set_capacity(capacity); // don't touch depo
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

EventTimeseriesChannel<StrategyInternalData> & StrategyInstance::strategy_internal_data_channel()
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

EventTimeseriesChannel<TpslFullPos::Prices> & StrategyInstance::tpsl_channel()
{
    return m_tpsl_channel;
}

EventTimeseriesChannel<StopLoss> & StrategyInstance::trailing_stop_channel()
{
    return m_trailing_stop_channel;
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

template <class T>
void StrategyInstance::handle_event_generic(const T & ev)
{
    handle_event(ev);
    after_every_event();
}

void StrategyInstance::handle_event(const HistoricalMDGeneratorEvent & response)
{
    if (m_historical_md_generator) {
        return;
    }

    const size_t erased_cnt = m_pending_requests.erase(response.request_guid());
    if (erased_cnt == 0) {
        Logger::logf<LogLevel::Debug>("unsolicited HistoricalMDPackEvent: {}, this->guid: {}", response.request_guid(), m_strategy_guid);
        return;
    }

    m_historical_md_generator = response;
    const auto ev_opt = m_historical_md_generator->get_next();
    if (!ev_opt.has_value()) {
        Logger::logf<LogLevel::Error>("no event in HistoricalMDGeneratorEvent: {}", response.request_guid());
        return;
    }

    m_backtest_in_progress = true;
    m_historical_md_channel.push(ev_opt.value());
}

void StrategyInstance::handle_event(const HistoricalMDPriceEvent & response)
{
    handle_event(static_cast<const MDPriceEvent &>(response));
    auto ev_opt = m_historical_md_generator->get_next();
    if (!ev_opt.has_value()) {
        m_stop_ev_channel.push({});
        m_historical_md_generator.reset();
        return;
    }
    const auto & ev = ev_opt.value();
    m_historical_md_channel.push(ev);
}

void StrategyInstance::handle_event(const MDPriceEvent & response)
{
    const auto & public_trade = response.public_trade;
    m_last_ts_and_price = {public_trade.ts(), public_trade.price()};
    m_price_channel.push(public_trade.ts(), public_trade.price());
    if (!first_price_received) {
        first_price_received = true;

        m_depo_channel.push(public_trade.ts(), 0.);
        m_strategy_result.update([&](auto & res) {
            res.strategy_start_ts = public_trade.ts();
        });
    }

    const auto candles = m_candle_builder.push_trade(public_trade.price(), public_trade.volume(), public_trade.ts());
    for (const auto & candle : candles) {
        m_candle_channel.push(candle.ts(), candle);
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

    if (pos.has_value() && m_strategy->export_price_levels()) {
        m_price_levels_channel.push(trade.ts(), pos->price_levels());
    }
    m_opened_pos_channel.push(m_position_manager.opened() != nullptr);

    m_trade_channel.push(trade.ts(), trade);
}

void StrategyInstance::handle_event(const StrategyStartRequest &)
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

void StrategyInstance::handle_event(const StrategyStopRequest &)
{
    Logger::log<LogLevel::Status>("StrategyStopRequest");
    close_position(m_last_ts_and_price.second, m_last_ts_and_price.first);

    m_live_md_requests.clear();

    m_backtest_in_progress = false;
    m_historical_md_generator.reset();
    m_stop_request_handled = true;
}

void StrategyInstance::close_position(double price, std::chrono::milliseconds ts)
{
    const auto * pos_ptr = m_position_manager.opened();
    if (pos_ptr == nullptr) {
        return;
    }
    const auto & pos = *pos_ptr;

    const auto [vol, side] = pos.opened_volume().as_unsigned_and_side();
    const auto volume = SignedVolume(vol, side.opposite());

    m_orders.send_market_order(price, volume, ts);
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
    if (m_stop_request_handled) {
        if (!m_stopped && ready_to_finish()) {
            m_stopped = true;
            m_finish_promise.set_value();
        }
        else {
            // std::cout << "Waiting for all criterias to be satisfied for finish" << std::endl;
            return;
        }
    }
}

void StrategyInstance::wait_event_barrier()
{
    EventBarrier b{m_event_loop, m_barrier_channel};
    b.wait();
}
