#pragma once

#define UNWRAP_RET_VOID(VAR, EXPR) \
    auto VAR##_PTR = EXPR;         \
    if (!VAR##_PTR) {              \
        return;                    \
    }                              \
    auto &(VAR) = *VAR##_PTR;

#define UNWRAP_CONTINUE(VAR, EXPR) \
    auto VAR##_PTR = EXPR;         \
    if (!VAR##_PTR) {              \
        continue;                  \
    }                              \
    auto &(VAR) = *VAR##_PTR;