#include "PythonStrategy.h"

#include "Logger.h"

#include <pybind11/embed.h>

#include <mutex>
#include <set>

namespace py = pybind11;

namespace {

class PythonInterpreter
{
public:
    static void ensure_initialized()
    {
        static PythonInterpreter interpreter;
    }

private:
    PythonInterpreter()
    {
        py::initialize_interpreter();
        py::module_::import("sys").attr("path").attr("insert")(0, std::string{PythonStrategy::python_strategies_dir});
        // Strategies run on many event loop threads, each of them acquires GIL on demand
        PyEval_SaveThread();
    }
};

// StrategyInternalData holds string_view, so names must live forever
std::string_view intern_chart_name(const std::string & name)
{
    static std::mutex mutex;
    static std::set<std::string> names;

    std::lock_guard lock(mutex);
    return *names.insert(name).first;
}

std::chrono::milliseconds to_ms(int64_t ms)
{
    return std::chrono::milliseconds{ms};
}

} // namespace

PYBIND11_EMBEDDED_MODULE(crypto, m)
{
    py::class_<Side>(m, "Side")
            .def_static("buy", &Side::buy)
            .def_static("sell", &Side::sell)
            .def("sign", &Side::sign)
            .def("opposite", &Side::opposite)
            .def("__eq__", [](const Side & lhs, const Side & rhs) { return lhs == rhs; })
            .def("__str__", &Side::to_string);

    py::enum_<MarketState>(m, "MarketState")
            .value("NONE", MarketState::None)
            .value("NO_TREND", MarketState::NoTrend)
            .value("UP_TREND", MarketState::UpTrend)
            .value("DOWN_TREND", MarketState::DownTrend)
            .value("HIGH_VOLATILITY", MarketState::HighVolatility);

    py::class_<Candle>(m, "Candle")
            .def_property_readonly("timeframe_ms", [](const Candle & c) { return c.timeframe().count(); })
            .def_property_readonly("ts_ms", [](const Candle & c) { return c.ts().count(); })
            .def_property_readonly("close_ts_ms", [](const Candle & c) { return c.close_ts().count(); })
            .def_property_readonly("open", &Candle::open)
            .def_property_readonly("high", &Candle::high)
            .def_property_readonly("low", &Candle::low)
            .def_property_readonly("close", &Candle::close)
            .def_property_readonly("side", &Candle::side)
            .def_property_readonly("volume", &Candle::volume)
            .def_property_readonly("buy_taker_volume", &Candle::buy_taker_volume)
            .def_property_readonly("sell_taker_volume", &Candle::sell_taker_volume)
            .def_property_readonly("trade_count", &Candle::trade_count);

    // Owned by C++, python gets only a reference
    py::class_<PythonStrategy, std::unique_ptr<PythonStrategy, py::nodelete>>(m, "StrategyContext")
            .def("try_send_order",
                 [](PythonStrategy & s, const Side & side, double price, int64_t ts_ms) {
                     return s.try_send_order(side, price, to_ms(ts_ms));
                 })
            .def("plot",
                 [](PythonStrategy & s, int64_t ts_ms, const std::string & chart_name, const std::string & series_name, double value) {
                     s.push_internal_data(to_ms(ts_ms), chart_name, series_name, value);
                 })
            .def("push_market_state",
                 [](PythonStrategy & s, int64_t candle_ts_ms, MarketState state) {
                     s.push_market_state(to_ms(candle_ts_ms), state);
                 });
}

GilSafePyObject::GilSafePyObject(py::object obj)
    : m_obj(std::move(obj))
{
}

GilSafePyObject & GilSafePyObject::operator=(py::object obj)
{
    m_obj = std::move(obj);
    return *this;
}

GilSafePyObject::~GilSafePyObject()
{
    if (!m_obj) {
        return;
    }

    py::gil_scoped_acquire gil;
    m_obj = py::object();
}

template <class... Args>
void PythonStrategy::call_python_handler(const char * handler_name, Args &&... args)
{
    py::gil_scoped_acquire gil;
    try {
        m_py_strategy.get().attr(handler_name)(std::forward<Args>(args)...);
    }
    catch (const py::error_already_set & e) {
        m_error_channel.push(std::make_pair(std::string{e.what()}, true));
    }
}

PythonStrategy::PythonStrategy(
        const std::string & module_name,
        const JsonStrategyConfig & config,
        EventLoop & event_loop,
        StrategyChannelsRefs channels,
        OrderManager & orders)
    : StrategyBase(orders, event_loop, channels)
    , m_exit_strategy(orders,
                      make_exit_strategy_config(config),
                      event_loop,
                      channels)
    , m_sub{event_loop}
{
    if (config.get().contains("timeframe_s")) {
        m_timeframe = std::chrono::seconds(config.get()["timeframe_s"].get<int>());
    }

    PythonInterpreter::ensure_initialized();
    py::gil_scoped_acquire gil;

    if (!create_python_strategy(module_name, config)) {
        return;
    }

    if (py::hasattr(m_py_strategy.get(), "on_price")) {
        m_sub.subscribe(
                channels.price_channel,
                [](const auto &) {},
                [this](const auto & ts, const double & price) {
                    call_python_handler("on_price", ts.count(), price);
                });
    }
    m_has_on_finish = py::hasattr(m_py_strategy.get(), "on_finish");
    if (py::hasattr(m_py_strategy.get(), "on_candle")) {
        m_sub.subscribe(
                channels.candle_channel,
                [](const auto &) {},
                [this](const auto &, const Candle & candle) {
                    // rvalue makes pybind move a copy to python, a reference would dangle if python keeps the candle
                    call_python_handler("on_candle", Candle{candle});
                });
    }
}

bool PythonStrategy::create_python_strategy(const std::string & module_name, const JsonStrategyConfig & config)
{
    try {
        const auto module = py::module_::import(module_name.c_str());
        const auto py_config = py::module_::import("json").attr("loads")(config.get().dump());
        m_py_strategy = module.attr("Strategy")(py::cast(this, py::return_value_policy::reference), py_config);

        m_is_valid = true;
        if (py::hasattr(m_py_strategy.get(), "is_valid")) {
            m_is_valid = m_py_strategy.get().attr("is_valid")().cast<bool>();
        }
    }
    catch (const py::error_already_set & e) {
        LOG_ERROR("Failed to create python strategy {}: {}", module_name, e.what());
        return false;
    }
    return m_is_valid;
}

JsonStrategyConfig PythonStrategy::make_exit_strategy_config(const JsonStrategyConfig & config)
{
    nlohmann::json json;
    json["risk"] = config.get().value("risk", 0.);
    json["no_loss_coef"] = config.get().value("no_loss_coef", 0.);
    return json;
}

void PythonStrategy::push_internal_data(
        std::chrono::milliseconds ts,
        const std::string & chart_name,
        const std::string & series_name,
        double value)
{
    m_strategy_internal_data_channel.push(ts, {intern_chart_name(chart_name), series_name, value});
}

void PythonStrategy::push_market_state(std::chrono::milliseconds candle_ts, MarketState state)
{
    m_market_state_channel.push(candle_ts, state);
}

void PythonStrategy::on_finish()
{
    if (m_has_on_finish) {
        call_python_handler("on_finish");
    }
}
