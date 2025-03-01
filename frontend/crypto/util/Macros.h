#pragma once

#define UNWRAP_RET_VOID(VAR, EXPR) \
    auto VAR##_PTR = EXPR;         \
    if (!VAR##_PTR) {              \
        return;                    \
    }                              \
    auto &(VAR) = *VAR##_PTR;

#define UNWRAP_RET_EMPTY(VAR, EXPR) \
    auto VAR##_PTR = EXPR;          \
    if (!VAR##_PTR) {               \
        return {};                  \
    }                               \
    auto &(VAR) = *VAR##_PTR;

#define UNWRAP_CONTINUE(VAR, EXPR) \
    auto VAR##_PTR = EXPR;         \
    if (!VAR##_PTR) {              \
        continue;                  \
    }                              \
    auto &(VAR) = *VAR##_PTR;

#define UNWRAP_RET(var, ret_value) \
    if (!var##_opt.has_value()) {  \
        return ret_value;          \
    }                              \
    const auto &(var) = var##_opt.value();
