#pragma once

#include <functional>

class ScopeExit
{
public:
    ScopeExit(std::function<void()> && func);
    ~ScopeExit();

private:
    std::function<void()> m_func;
};

