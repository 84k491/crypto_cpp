#pragma once

#include "MarketOrder.h"
#include "Volume.h"
#include "crossguid/guid.hpp"

#include <chrono>
#include <string>
#include <utility>

class ConditionalMarketOrder : public MarketOrder
{
public:
    enum class Type : uint8_t
    {
        StopLoss,
        TakeProfit,
    };

    ConditionalMarketOrder(
            std::string symbol,
            double price,
            UnsignedVolume volume,
            Side side,
            Type conditional_type,
            std::chrono::milliseconds signal_ts)
        : MarketOrder(
                  std::move(symbol),
                  price,
                  std::move(volume),
                  side,
                  signal_ts)
        , m_guid(xg::newGuid())
        , m_trigger_price(price)
        , m_type(conditional_type)
    {
    }

    auto & trigger_price() const { return m_trigger_price; }
    auto & type() const { return m_type; }
    OrderStatus status() const override;
    void on_state_changed(bool active);
    UnsignedVolume suspended_volume() const { return m_suspended_volume; }

    void cancel();
    bool is_cancel_requested() const { return m_cancel_requested; }

    void on_trade(UnsignedVolume vol, double price, double fee) override;

private:
    bool m_cancel_requested = false;
    xg::Guid m_guid;
    UnsignedVolume m_suspended_volume;
    double m_trigger_price = 0.;
    Type m_type;
};

class StopLossMarketOrder : public ConditionalMarketOrder
{
public:
    StopLossMarketOrder(
            std::string symbol,
            double price,
            UnsignedVolume volume,
            Side side,
            std::chrono::milliseconds signal_ts)
        : ConditionalMarketOrder(
                  std::move(symbol),
                  price,
                  std::move(volume),
                  side,
                  ConditionalMarketOrder::Type::StopLoss,
                  signal_ts)
    {
    }
};

class TakeProfitMarketOrder : public ConditionalMarketOrder
{
public:
    TakeProfitMarketOrder(
            std::string symbol,
            double price,
            UnsignedVolume volume,
            Side side,
            std::chrono::milliseconds signal_ts)
        : ConditionalMarketOrder(
                  std::move(symbol),
                  price,
                  std::move(volume),
                  side,
                  ConditionalMarketOrder::Type::TakeProfit,
                  signal_ts)
    {
    }
};

class TpslFullPos
{
public:
    struct Prices
    {
        double take_profit_price = 0.;
        double stop_loss_price = 0.;
    };

    TpslFullPos(
            std::string symbol,
            double take_profit_price,
            double stop_loss_price,
            Side side,
            std::chrono::milliseconds signal_ts);

    OrderStatus status() const;

    double take_profit_price() const;
    double stop_loss_price() const;
    xg::Guid guid() const;

    std::optional<std::string> m_reject_reason;

    bool m_set_up = false;
    bool m_triggered = false;
    bool m_acked = false;

private:
    std::string m_symbol;
    xg::Guid m_guid;

    Prices m_prices;

    Side m_side;
};
