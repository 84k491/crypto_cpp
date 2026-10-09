"""
Model features. Candle i features use only candles [0, i], no future data.
Needs at least `max_window` past candles.

All returns are log returns: log(close[i] / close[j]).

return_1, return_5, return_20
    Price change over the last 1, 5, 20 candles. Direction and speed of the recent move.

volatility_20, volatility_100
    Std of 1-candle log returns over the last 20 and 100 candles. How noisy the price is.

volatility_ratio
    volatility_20 / volatility_100. Above 1: volatility is rising, below 1: market calms down.

efficiency_20
    |close[i] - close[i - 20]| / sum of |close[j] - close[j - 1]| over the last 20 candles.
    1 is a straight move, close to 0 is back-and-forth chop.

sma_distance_20, sma_distance_50
    close / SMA(close over the last 20, 50 candles) - 1. Positive: price above its average.

volume_imbalance
    (buy_taker_volume - sell_taker_volume) / volume of the candle, from -1 to 1.
    Positive: aggressive buyers dominate.

range
    (high - low) / close. Candle size relative to price.

body
    (close - open) / open. Signed candle body: positive for a green candle.

trade_count
    Number of trades in the candle. Raw value, depends on the coin and timeframe.

volume
    buy_taker_volume + sell_taker_volume of the candle. Raw value, depends on the coin and timeframe.
"""

import math

from regime.labeling import log_returns
from regime.prefix_sums import PrefixSums

RETURN_WINDOWS = (1, 5, 20)
VOLATILITY_WINDOWS = (20, 100)
EFFICIENCY_WINDOW = 20
SMA_WINDOWS = (20, 50)

max_window = max(*RETURN_WINDOWS, *VOLATILITY_WINDOWS, EFFICIENCY_WINDOW, *SMA_WINDOWS)


def feature_names():
    return (
        [f"return_{w}" for w in RETURN_WINDOWS]
        + [f"volatility_{w}" for w in VOLATILITY_WINDOWS]
        + ["volatility_ratio", f"efficiency_{EFFICIENCY_WINDOW}"]
        + [f"sma_distance_{w}" for w in SMA_WINDOWS]
        + ["volume_imbalance", "range", "body", "trade_count", "volume"]
    )


class FeatureBuilder:
    def __init__(self, candles):
        self.candles = candles
        self.closes = [c.close for c in candles]
        self.returns = PrefixSums(log_returns(self.closes))
        self.closes_sums = PrefixSums(self.closes)
        self.path = PrefixSums([0.0] + [abs(self.closes[j] - self.closes[j - 1]) for j in range(1, len(self.closes))])

    def build(self, i):
        c = self.candles[i]
        close = self.closes[i]
        volatilities = [self.returns.std(i - w + 1, i + 1) for w in VOLATILITY_WINDOWS]
        volume = c.buy_taker_volume + c.sell_taker_volume

        path_len = self.path.sum(i - EFFICIENCY_WINDOW + 1, i + 1)
        efficiency = abs(close - self.closes[i - EFFICIENCY_WINDOW]) / path_len if path_len > 0 else 0.0

        return (
            [math.log(close / self.closes[i - w]) for w in RETURN_WINDOWS]
            + volatilities
            + [volatilities[0] / volatilities[1] if volatilities[1] > 0 else 0.0, efficiency]
            + [close / self.closes_sums.mean(i - w + 1, i + 1) - 1 for w in SMA_WINDOWS]
            + [
                (c.buy_taker_volume - c.sell_taker_volume) / volume if volume > 0 else 0.0,
                (c.high - c.low) / close,
                (c.close - c.open) / c.open,
                c.trade_count,
                volume,
            ]
        )
