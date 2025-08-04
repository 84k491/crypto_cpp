#pragma once

#include "nlohmann/json.hpp"

#include <utility>

class JsonStrategyConfig
{
    friend std::ostream & operator<<(std::ostream & os, const JsonStrategyConfig & config);

public:
    JsonStrategyConfig(nlohmann::json config)
        : m_config(std::move(config))
    {
    }

    const auto & get() const { return m_config; }

private:
    nlohmann::json m_config;
};
std::ostream & operator<<(std::ostream & os, const JsonStrategyConfig & config);

class JsonStrategyMetaInfo
{
public:
    JsonStrategyMetaInfo(nlohmann::json meta_info)
        : m_meta_info(std::move(meta_info))
    {
    }

    const auto & get() const { return m_meta_info; }
    bool got_parameters() const { return m_meta_info.find("parameters") != m_meta_info.end(); }

private:
    nlohmann::json m_meta_info;
};
