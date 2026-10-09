"""
Deterministic market regime markup based on price swings (zigzag).
Looks into the future, use it for training targets only.

1. Swing pivots: a high (low) becomes a pivot when price moves back from it by `reversal` (log return).
   Price between two neighbour pivots is a swing.
2. Each swing [a, b) is labeled as a whole:
    sigma_base    std of log returns over `baseline` candles before the swing
    sigma_swing   std of log returns inside the swing
    swing_return  log(close[b] / close[a])

    sigma_swing > vol_coef * sigma_base                             -> HIGH_VOLATILITY
    |swing_return| > trend_coef * sigma_swing * sqrt(swing length)  -> UP_TREND / DOWN_TREND
    otherwise                                                       -> NO_TREND

Trend noise uses the swing's own volatility: the period before a swing may be much more volatile
(e.g. a crash before a calm grind up), and would hide the trend.

Not labeled: swings that start before `baseline` candles, and the last swing, because its end isn't confirmed.
"""

import math
from dataclasses import dataclass

from regime.prefix_sums import PrefixSums

HIGH_VOLATILITY = "HIGH_VOLATILITY"
UP_TREND = "UP_TREND"
DOWN_TREND = "DOWN_TREND"
NO_TREND = "NO_TREND"


@dataclass
class LabelingParams:
    reversal: float
    trend_coef: float
    baseline: int
    vol_coef: float


@dataclass
class SwingStats:
    sigma_base: float
    sigma_swing: float
    swing_return: float
    length: int

    def volatility_ratio(self):
        return self.sigma_swing / self.sigma_base if self.sigma_base > 0 else 0.0

    def trend_strength(self):
        """swing_return in units of a random move over the same length"""
        noise = self.sigma_swing * math.sqrt(self.length)
        return self.swing_return / noise if noise > 0 else 0.0


def log_returns(closes):
    """log_returns[j] is the return of candle j, log_returns[0] is 0."""
    return [0.0] + [math.log(closes[j] / closes[j - 1]) for j in range(1, len(closes))]


def find_swing_pivots(closes, reversal):
    pivots = []
    high = low = 0
    direction = 0  # 1: up swing in progress, -1: down swing, 0: not known yet

    for i in range(1, len(closes)):
        if closes[i] > closes[high]:
            high = i
        if closes[i] < closes[low]:
            low = i

        if direction >= 0 and math.log(closes[high] / closes[i]) >= reversal:
            if direction == 0 and low < high:
                pivots.append(low)
            pivots.append(high)
            direction = -1
            low = i
        elif direction <= 0 and math.log(closes[i] / closes[low]) >= reversal:
            if direction == 0 and high < low:
                pivots.append(high)
            pivots.append(low)
            direction = 1
            high = i

    return pivots


def swing_stats(returns, closes, begin, end, baseline):
    return SwingStats(
        sigma_base=returns.std(begin - baseline + 1, begin + 1),
        sigma_swing=returns.std(begin + 1, end + 1),
        swing_return=math.log(closes[end] / closes[begin]),
        length=end - begin,
    )


def swing_label(stats, params):
    if stats.sigma_swing > params.vol_coef * stats.sigma_base:
        return HIGH_VOLATILITY
    if stats.trend_strength() > params.trend_coef:
        return UP_TREND
    if stats.trend_strength() < -params.trend_coef:
        return DOWN_TREND
    return NO_TREND


def markup(closes, params):
    """Returns {candle index: label} and {candle index: SwingStats of its swing}, ordered by index."""
    returns = PrefixSums(log_returns(closes))
    pivots = find_swing_pivots(closes, params.reversal)

    labels = {}
    stats = {}
    for begin, end in zip(pivots, pivots[1:]):
        if begin < params.baseline:
            continue
        swing = swing_stats(returns, closes, begin, end, params.baseline)
        label = swing_label(swing, params)
        for i in range(begin, end):
            labels[i] = label
            stats[i] = swing
    return labels, stats
