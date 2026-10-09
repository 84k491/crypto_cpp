#pragma once

#include "Candle.h"
#include "ExitStrategyInterface.h"
#include "JsonStrategyConfig.h"
#include "StrategyBase.h"
#include "StrategyChannels.h"

#include <pybind11/pybind11.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>

// Python objects must be destroyed with the GIL held
class GilSafePyObject
{
public:
    GilSafePyObject() = default;
    GilSafePyObject(pybind11::object obj);
    GilSafePyObject & operator=(pybind11::object obj);
    ~GilSafePyObject();

    pybind11::object & get() { return m_obj; }

private:
    pybind11::object m_obj;
};

/*
    Runs a strategy written in Python: ./python_strategies/<name>.py
    The module must define a class "Strategy":

        class Strategy:
            def __init__(self, ctx: crypto.StrategyContext, config: dict): ...
            def is_valid(self) -> bool: ...                       # optional
            def on_price(self, ts_ms: int, price: float): ...     # optional
            def on_candle(self, candle: crypto.Candle): ...       # optional
            def on_finish(self): ...                              # optional

    Exit strategy is chosen by optional attribute of the python strategy:
        exit_strategy = "DynamicTrailingStopLoss"  # default, config needs "risk" and "no_loss_coef"
        exit_strategy = "Tpsl"                     # config needs "risk" and "risk_reward_ratio"
    Candle timeframe is taken from "timeframe_s" config field.
*/
class PythonStrategy final : public StrategyBase
{
public:
    static constexpr std::string_view python_strategies_dir = "./python_strategies";

    PythonStrategy(
            const std::string & module_name,
            const JsonStrategyConfig & config,
            EventLoop & event_loop,
            StrategyChannelsRefs channels,
            OrderManager & orders);

    bool is_valid() const override { return m_is_valid; }
    std::optional<std::chrono::milliseconds> timeframe() const override { return m_timeframe; }
    void on_finish() override;

    using StrategyBase::try_send_order;
    void push_internal_data(
            std::chrono::milliseconds ts,
            const std::string & chart_name,
            const std::string & series_name,
            double value);
    void push_market_state(std::chrono::milliseconds candle_ts, MarketState state);

private:
    static JsonStrategyConfig make_dynamic_trailing_stop_loss_config(const JsonStrategyConfig & config);
    bool create_python_strategy(const std::string & module_name, const JsonStrategyConfig & config);
    bool create_exit_strategy(
            const std::string & exit_strategy_name,
            const JsonStrategyConfig & config,
            EventLoop & event_loop,
            StrategyChannelsRefs channels,
            OrderManager & orders);

    template <class... Args>
    void call_python_handler(const char * handler_name, Args &&... args);

private:
    bool m_is_valid = false;
    std::optional<std::chrono::milliseconds> m_timeframe;
    bool m_has_on_finish = false;

    std::unique_ptr<IExitStrategy> m_exit_strategy;

    GilSafePyObject m_py_strategy;

    EventSubcriber m_sub;
};
