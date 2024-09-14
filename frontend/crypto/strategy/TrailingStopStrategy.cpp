#include "TrailingStopStrategy.h"

#include "Logger.h"

#include <utility>

TrailigStopLossStrategyConfig::TrailigStopLossStrategyConfig(const JsonStrategyConfig & config)
{
    if (config.get().contains("risk")) {
        m_risk = config.get()["risk"].get<double>();
    }
    Logger::log<LogLevel::Info>("TrailigStopLossStrategyConfig c-tor");
}

TrailigStopLossStrategyConfig::TrailigStopLossStrategyConfig(double risk)
    : m_risk(risk)
{
}

TrailigStopLossStrategy::TrailigStopLossStrategy(Symbol symbol,
                                                 JsonStrategyConfig config,
                                                 std::shared_ptr<EventLoop<STRATEGY_EVENTS>> & event_loop,
                                                 ITradingGateway & gateway)

    : ExitStrategyBase(gateway)
    , m_symbol(std::move(symbol))
    , m_event_loop(event_loop)
    , m_config(config)
{
}

std::optional<std::string> TrailigStopLossStrategy::on_price_changed(std::pair<std::chrono::milliseconds, double> ts_and_price)
{
    m_last_ts_and_price = std::move(ts_and_price);
    return std::nullopt;
}

std::optional<std::string> TrailigStopLossStrategy::on_trade(const std::optional<OpenedPosition> & opened_position, const Trade & trade)
{
    if (m_opened_position.has_value() && m_active_stop.has_value()) {
        const std::string_view msg = "TpslExitStrategy: active tpsl already exists";
        Logger::log<LogLevel::Warning>(std::string(msg));
        return std::string(msg);
    }

    m_opened_position = opened_position;

    // TODO handle a double trade case, add test
    if (opened_position) {
        const auto tpsl = calc_trailing_stop(trade);
        send_trailing_stop(tpsl);
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

    const auto stop_loss = TrailingStopLoss(trade.symbol(), price_delta, trade.side());
    return stop_loss;
}

void TrailigStopLossStrategy::send_trailing_stop(TrailingStopLoss trailing_stop)
{
    TrailingStopLossRequestEvent request(
            m_symbol,
            std::move(trailing_stop),
            m_event_loop);
    m_pending_requests.emplace(request.guid);
    m_tr_gateway.push_trailing_stop_request(request);
}

std::optional<std::pair<std::string, bool>> TrailigStopLossStrategy::handle_event(const TpslResponseEvent &)
{
    return {{"TpslResponse in stop loss", true}};
}

std::optional<std::pair<std::string, bool>> TrailigStopLossStrategy::handle_event(const TpslUpdatedEvent &)
{
    return {{"TpslUpdate in stop loss", true}};
}

std::optional<std::pair<std::string, bool>> TrailigStopLossStrategy::handle_event(const TrailingStopLossResponseEvent & response)
{
    if (response.reject_reason.has_value()) {
        std::string err = "Rejected tpsl: " + response.reject_reason.value();
        return {{err, true}};
    }

    const size_t erased_cnt = m_pending_requests.erase(response.request_guid);
    if (erased_cnt == 0) {
        std::string err = "Unsolicited tpsl response";
        return {{err, false}};
    }

    m_trailing_stop_publisher.push(m_last_ts_and_price.first, response.trailing_stop_loss);
    return std::nullopt;
}

std::optional<std::pair<std::string, bool>> TrailigStopLossStrategy::handle_event(const TrailingStopLossUpdatedEvent &)
{
    Logger::log<LogLevel::Debug>("TrailingStopLossUpdatedEvent"); // TODO print it out
    return std::nullopt;
}
