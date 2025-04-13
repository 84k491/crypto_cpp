#include "TrailingStopStrategy.h"

#include "EventLoop.h"
#include "Logger.h"

#include <utility>

TrailigStopLossStrategyConfig::TrailigStopLossStrategyConfig(const JsonStrategyConfig & config)
{
    if (config.get().contains("risk")) {
        m_risk = config.get()["risk"].get<double>();
    }
    Logger::log<LogLevel::Debug>("TrailigStopLossStrategyConfig c-tor");
}

TrailigStopLossStrategyConfig::TrailigStopLossStrategyConfig(double risk)
    : m_risk(risk)
{
}

TrailigStopLossStrategy::TrailigStopLossStrategy(Symbol symbol,
                                                 JsonStrategyConfig config,
                                                 EventLoopSubscriber<STRATEGY_EVENTS> & event_loop,
                                                 ITradingGateway & gateway)

    : ExitStrategyBase(gateway)
    , m_symbol(std::move(symbol))
    , m_config(config)
{
    m_invoker_subs.push_back(
            event_loop.invoker().register_invoker<TrailingStopLossResponseEvent>([&](const auto & response) {
                handle_event(response);
            }));
    m_invoker_subs.push_back(
            event_loop.invoker().register_invoker<TrailingStopLossUpdatedEvent>([&](const auto & response) {
                handle_event(response);
            }));
}

std::optional<std::string> TrailigStopLossStrategy::on_price_changed(std::pair<std::chrono::milliseconds, double>)
{
    return std::nullopt;
}

std::optional<std::string> TrailigStopLossStrategy::on_trade(const std::optional<OpenedPosition> & opened_position, const Trade & trade)
{
    if (opened_position && (m_active_stop_loss.has_value() || !m_pending_requests.empty())) {
        // position opened
        // stop loss is set already
        const std::string_view msg = "TrailigStopLossStrategy: active stop loss already exists or pending";
        Logger::log<LogLevel::Warning>(std::string(msg));
        return std::string(msg);
    }

    // TODO handle a double trade case, add test
    if (opened_position) {
        const auto tsl = calc_trailing_stop(trade);
        send_trailing_stop(tsl);
    }
    return std::nullopt;
}

TrailingStopLoss TrailigStopLossStrategy::calc_trailing_stop(const Trade & trade)
{
    const double & entry_price = trade.price();
    const double total_fee = trade.fee() * 2;
    const double fee_price_delta = total_fee / trade.unsigned_volume().value();

    const double risk = entry_price * m_config.risk();
    const double price_delta = risk - fee_price_delta;

    const auto opposite_side = trade.side().opposite();
    const auto stop_loss = TrailingStopLoss(trade.symbol_name(), price_delta, opposite_side);
    return stop_loss;
}

void TrailigStopLossStrategy::send_trailing_stop(TrailingStopLoss trailing_stop)
{
    TrailingStopLossRequestEvent request(
            m_symbol,
            std::move(trailing_stop));
    m_pending_requests.emplace(request.guid);
    m_tr_gateway.push_trailing_stop_request(request);
}

void TrailigStopLossStrategy::handle_event(const TrailingStopLossResponseEvent & response)
{
    if (response.reject_reason.has_value()) {
        std::string err = "Rejected tpsl: " + response.reject_reason.value();
        on_error(err, false);
    }

    const size_t erased_cnt = m_pending_requests.erase(response.request_guid);
    if (erased_cnt == 0) {
        std::string err = "Unsolicited tpsl response";
        on_error(err, false);
    }

    m_active_stop_loss = response.trailing_stop_loss;
}

void TrailigStopLossStrategy::handle_event(const TrailingStopLossUpdatedEvent & ev)
{
    // Logger::log<LogLevel::Debug>("Received TrailingStopLossUpdatedEvent"); // TODO print it out
    if (!ev.stop_loss.has_value()) {
        m_active_stop_loss.reset();
        Logger::log<LogLevel::Debug>("Trailing stop loss removed");
        return;
    }
    const auto & trailing_stop_loss = ev.stop_loss.value();

    m_trailing_stop_channel.push(ev.timestamp, trailing_stop_loss);
}

void TrailigStopLossStrategy::on_error(const std::string & err, bool do_panic)
{
    m_error_channel.push(std::make_pair(err, do_panic));
}
