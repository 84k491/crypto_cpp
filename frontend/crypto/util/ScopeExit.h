#pragma once

#include <functional>

class ScopeExit
{
public:
    ScopeExit(std::function<void()> && func);
    ~ScopeExit();

    void cancel();

private:
    std::function<void()> m_func;

    bool m_is_cancelled = false;
};

