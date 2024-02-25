#include "ScopeExit.h"

ScopeExit::ScopeExit(std::function<void()> && func)
    : m_func(std::move(func))
{
}

void ScopeExit::cancel()
{
    m_is_cancelled = true;
}

ScopeExit::~ScopeExit()
{
    if (!m_is_cancelled) {
        m_func();
    }
}
