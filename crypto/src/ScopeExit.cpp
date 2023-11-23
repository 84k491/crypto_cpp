#include "ScopeExit.h"

ScopeExit::ScopeExit(std::function<void()> && func)
    : m_func(std::move(func))
{
}
ScopeExit::~ScopeExit() { m_func(); }
