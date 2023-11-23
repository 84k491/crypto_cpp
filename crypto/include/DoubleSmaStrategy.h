#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <list>
#include <optional>
#include <ratio>

enum class Side
{
    Buy,
    Sell,
};

struct Signal
{
    Side side;
    double price;
};

class ScopeExit
{
public:
    ScopeExit(std::function<void()> && func)
        : m_func(std::move(func))
    {
    }
    ~ScopeExit() { m_func(); }

private:
    std::function<void()> m_func;
};

class DoubleSmaStrategy
{
public:
    DoubleSmaStrategy(std::chrono::milliseconds slow_interval, std::chrono::milliseconds fast_interval);

    std::optional<Signal> push_price(std::pair<uint64_t, double> ts_and_price);

private:
    std::chrono::milliseconds m_slow_interval{};
    std::chrono::milliseconds m_fast_interval{};

    std::list<std::pair<uint64_t, double>> m_slow_data;
    std::list<std::pair<uint64_t, double>> m_fast_data;

    std::optional<double> m_prev_slow_avg{};
    std::optional<double> m_prev_fast_avg{};
};
