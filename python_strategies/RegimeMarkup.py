"""
Doesn't trade. Collects candles, and when the run finishes:
  - labels every candle with a market regime (see regime/labeling.py)
  - shows the labels on the price chart
  - writes candles, features and labels to markup/RegimeMarkup_<first_ts>_<last_ts>.csv
"""

import csv
import os

import crypto

from regime import features
from regime.labeling import LabelingParams, markup

MARKUP_DIR = "markup"

TO_MARKET_STATE = {
    "HIGH_VOLATILITY": crypto.MarketState.HIGH_VOLATILITY,
    "UP_TREND": crypto.MarketState.UP_TREND,
    "DOWN_TREND": crypto.MarketState.DOWN_TREND,
    "NO_TREND": crypto.MarketState.NO_TREND,
}


class Strategy:
    def __init__(self, ctx, config):
        self.ctx = ctx
        self.params = LabelingParams(
            reversal=config["reversal_perc"] / 100.0,
            trend_coef=config["trend_coef"],
            baseline=int(config["baseline"]),
            vol_coef=config["vol_coef"],
        )
        self.candles = []

    def is_valid(self):
        return self.params.reversal > 0 and self.params.trend_coef > 0 and self.params.baseline >= features.max_window

    def on_candle(self, candle):
        self.candles.append(candle)

    def on_finish(self):
        labels, stats = markup([c.close for c in self.candles], self.params)
        if not labels:
            return

        for i, label in labels.items():
            ts_ms = self.candles[i].ts_ms
            self.ctx.push_market_state(ts_ms, TO_MARKET_STATE[label])
            self.ctx.plot(ts_ms, "regime_markup", "trend_strength", stats[i].trend_strength())
            self.ctx.plot(ts_ms, "regime_markup", "volatility_ratio", stats[i].volatility_ratio())

        self.write_csv(labels)

    def write_csv(self, labels):
        os.makedirs(MARKUP_DIR, exist_ok=True)
        path = os.path.join(MARKUP_DIR, f"RegimeMarkup_{self.candles[0].ts_ms}_{self.candles[-1].ts_ms}.csv")

        feature_builder = features.FeatureBuilder(self.candles)
        with open(path, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(["ts_ms", "open", "high", "low", "close"] + features.feature_names() + ["label"])
            for i, label in labels.items():
                c = self.candles[i]
                writer.writerow([c.ts_ms, c.open, c.high, c.low, c.close] + feature_builder.build(i) + [label])
