#pragma once

#include "Candle.h"
#include "EventObjectChannel.h"
#include "EventTimeseriesChannel.h"
#include "Position.h"

struct StrategyChannelsRefs
{
    EventTimeseriesChannel<double> & price_channel;
    EventTimeseriesChannel<Candle> & candle_channel;
    EventObjectChannel<bool> & opened_pos_channel;

    EventTimeseriesChannel<Trade> & trades_channel;
    EventTimeseriesChannel<ProfitPriceLevels> & price_levels_channel;

    EventTimeseriesChannel<StopLoss> & trailing_stop_loss_channel;
};
