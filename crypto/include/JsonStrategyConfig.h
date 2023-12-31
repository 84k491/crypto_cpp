#pragma once

#include "nlohmann/json.hpp"

#include <utility>

class JsonStrategyConfig
{
public:
    JsonStrategyConfig(nlohmann::json config)
        : m_config(std::move(config))
    {
    }

    const auto & get() const { return m_config; }

private:
    nlohmann::json m_config;
};

class JsonStrategyMetaInfo
{
public:
    JsonStrategyMetaInfo(nlohmann::json meta_info)
        : m_meta_info(std::move(meta_info))
    {
    }

    const auto & get() const { return m_meta_info; }

private:
    nlohmann::json m_meta_info;
};
