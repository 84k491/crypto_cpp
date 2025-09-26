#pragma once

#include "JsonStrategyConfig.h"
#include "OrderManager.h"
#include "StrategyChannels.h"
#include "StrategyInterface.h"

#include <memory>

class StrategyFactory
{
    StrategyFactory();

public:
    static StrategyFactory & i();

    std::vector<std::string> get_all_strategy_names() const;

    std::optional<JsonStrategyMetaInfo> get_meta_info(
            const std::string & strategy_name) const;
    std::optional<std::shared_ptr<IStrategy>> build_strategy(
            const std::string & strategy_name,
            const JsonStrategyConfig & config,
            std::shared_ptr<EventLoop> & event_loop,
            StrategyChannelsRefs channels,
            OrderManager & orders) const;

private:
    using BuilderFunction = std::function<
            std::optional<std::shared_ptr<IStrategy>>(
                    const JsonStrategyConfig &,
                    std::shared_ptr<EventLoop> &,
                    StrategyChannelsRefs,
                    OrderManager &)>;

    std::map<std::string, BuilderFunction> m_builders;
};
