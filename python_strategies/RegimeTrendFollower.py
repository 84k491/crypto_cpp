"""
Follows a solid trend predicted by the regime model (see RegimeClassifierTraining.py).

The newest models/RegimeModel_*.txt is loaded. On every candle the model gives label probabilities.
A candle is a solid uptrend (downtrend) if P(UP_TREND) (P(DOWN_TREND)) >= min_trend_probability.
When solid candles of the same direction last for trend_duration_m minutes in a row, a position is opened
in the trend direction. Exit is Tpsl.

While a position is opened, new orders are refused. After the exit the position is reopened right away
if the trend is still solid.
"""

import functools

import crypto

from regime import features, labeling, model

MODELS_DIR = "models"

UP = labeling.LABELS.index(labeling.UP_TREND)
DOWN = labeling.LABELS.index(labeling.DOWN_TREND)


@functools.lru_cache(maxsize=None)
def load_model(path):
    """Cached: optimizer creates many strategy instances with the same model."""
    return model.load_model(path)


class Strategy:
    exit_strategy = "Tpsl"

    def __init__(self, ctx, config):
        self.ctx = ctx
        self.trend_duration_ms = config["trend_duration_m"] * 60_000
        self.min_trend_probability = config["min_trend_probability"]

        model_path = model.latest_model_path(MODELS_DIR)
        if model_path is None:
            raise FileNotFoundError(f"No models in {MODELS_DIR}, run RegimeClassifierTraining first")
        self.model = load_model(model_path)
        print(f"RegimeTrendFollower: using model {model_path}.txt", flush=True)

        self.sides = {UP: crypto.Side.buy(), DOWN: crypto.Side.sell()}
        self.candles = []
        self.trend_direction = None
        self.trend_start_ts_ms = None

    def is_valid(self):
        return self.trend_duration_ms > 0 and 0 < self.min_trend_probability < 1

    def on_candle(self, candle):
        self.candles.append(candle)
        if len(self.candles) <= features.max_window:
            return
        del self.candles[: -(features.max_window + 1)]

        probabilities = self.model.predict([features.FeatureBuilder(self.candles).build(features.max_window)], num_threads=1)[0]
        ts_ms = candle.close_ts_ms
        self.ctx.plot(ts_ms, "regime_model", "UP_TREND", probabilities[UP])
        self.ctx.plot(ts_ms, "regime_model", "DOWN_TREND", probabilities[DOWN])
        self.ctx.plot(ts_ms, "regime_model", "min_trend_probability", self.min_trend_probability)

        self.update_trend(probabilities, candle.ts_ms)
        if self.trend_direction is None or ts_ms - self.trend_start_ts_ms < self.trend_duration_ms:
            return
        self.ctx.try_send_order(self.sides[self.trend_direction], candle.close, ts_ms)

    def update_trend(self, probabilities, candle_ts_ms):
        direction = None
        if probabilities[UP] >= self.min_trend_probability:
            direction = UP
        elif probabilities[DOWN] >= self.min_trend_probability:
            direction = DOWN

        if direction != self.trend_direction:
            self.trend_direction = direction
            self.trend_start_ts_ms = candle_ts_ms
