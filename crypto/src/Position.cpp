#include "Position.h"

#include "Types.h"

#include <iostream>
#include <optional>
#include <utility>

bool PositionManager::open(SignedVolume target_absolute_volume)
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

    if (m_opened_position.has_value()) {
        std::cout << "ERROR: opening already opened position" << std::endl;
        return false;
    }

    if (m_tr_gateway == nullptr) {
        return true;
    }
    const auto order = MarketOrder{
            m_symbol.symbol_name,
            adjusted_target_volume};

    const auto trades_opt = m_tr_gateway->send_order_sync(order);
    if (!trades_opt.has_value()) {
        std::cout << "ERROR Failed to send an order" << std::endl;
        return false;
    }
    for (const auto & trade : trades_opt.value()) {
        on_trade(trade);
        // TODO check for absence of result
    }

    return true;
}

std::optional<PositionResult> PositionManager::close()
{
    if (!m_opened_position.has_value()) {
        return std::nullopt;
    }
    auto & pos = m_opened_position.value();

    const auto order = MarketOrder{
            m_symbol.symbol_name,
            pos.absolute_volume()};

    const auto trades_opt = m_tr_gateway->send_order_sync(order);
    if (!trades_opt.has_value()) {
        std::cout << "ERROR Failed to send order" << std::endl;
        return std::nullopt;
    }
    std::optional<PositionResult> overall_result;
    for (const auto & trade : trades_opt.value()) {
        const auto res_opt = on_trade(trade);
        if (res_opt.has_value()) {
            if (overall_result.has_value()) {
                std::cout << "ERROR: two position results for a single order" << std::endl;
            }
            overall_result = res_opt;
        }
    }
    if (!overall_result.has_value()) {
        std::cout << "ERROR: no position result" << std::endl;
    }
    return overall_result;
}

std::optional<PositionResult> PositionManager::on_trade(const Trade & trade)
{
    if (!m_opened_position.has_value()) {
        m_opened_position = OpenedPosition(trade.ts, SignedVolume(trade.unsigned_volume, trade.side), trade.price);
        return std::nullopt;
    }

    const auto & pos = m_opened_position.value();

    PositionResult res;
    const auto pnl = (trade.price - pos.open_price()) * pos.absolute_volume().value();
    res.pnl = pnl;
    res.opened_time = trade.ts - pos.open_ts();

    m_opened_position = {};
    return res;
}

Side PositionManager::OpenedPosition::side() const
{
    return m_absolute_volume.as_unsigned_and_side().second;
}

PositionManager::OpenedPosition::OpenedPosition(std::chrono::milliseconds ts, SignedVolume absolute_volume, double price)
    : m_absolute_volume(absolute_volume)
    , m_open_price(price)
    , m_open_ts(ts)
{
}
