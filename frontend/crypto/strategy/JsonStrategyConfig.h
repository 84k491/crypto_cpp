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

class DoubleJsonStrategyConfig
{
public:
    DoubleJsonStrategyConfig(JsonStrategyConfig entry_config, JsonStrategyConfig exit_config)
        : m_entry_config(std::move(entry_config))
        , m_exit_config(std::move(exit_config))
    {
    }

    DoubleJsonStrategyConfig(nlohmann::json double_config)
        : DoubleJsonStrategyConfig(double_config["entry"], double_config["exit"])
    {
    }

    nlohmann::json to_json() const
    {
        return {{"entry", m_entry_config.get()}, {"exit", m_exit_config.get()}};
    }

public:
    JsonStrategyConfig m_entry_config;
    JsonStrategyConfig m_exit_config;
};
