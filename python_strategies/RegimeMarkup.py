"""
Doesn't trade. Collects candles, and when the run finishes:
  - labels every candle with a market regime (see regime/labeling.py)
  - shows the labels on the price chart
  - writes candles, features and labels to markup/RegimeMarkup_<first_ts>_<last_ts>.csv
"""

import csv
import os

from regime import features
from regime.labeling import LabelingParams, markup
from regime.market_states import TO_MARKET_STATE

MARKUP_DIR = "markup"


class Strategy:
    def __init__(self, ctx, config):
        self.ctx = ctx
        self.params = LabelingParams.from_strategy_config(config)
        self.candles = []

    def is_valid(self):
        return self.params.is_valid() and self.params.baseline >= features.max_window

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
