"""
Doesn't trade. When the run finishes:
  - trains a LightGBM regime classifier on all markup/*.csv files made by RegimeMarkup
  - walk-forward validation over those files (see regime/model.py), prints a report per fold
  - trains the final model on all rows and saves it to models/RegimeModel_<first_ts>_<last_ts>.txt
    with a .json next to it: labels order, features, parameters, markup files and validation reports
  - runs the model on the run's candles: label probabilities on "regime_model" chart,
    the most probable label as a market state on the price chart (not a decision, it ignores confidence).
    Run it on a period that is not in the markup files, otherwise the results are in-sample.
"""

import json
import os
from dataclasses import asdict

from regime import features, labeling, model
from regime.dataset import load_markup_dataset, markup_files
from regime.market_states import TO_MARKET_STATE
from regime.model import TrainingParams

MARKUP_DIR = "markup"
MODELS_DIR = "models"


class Strategy:
    def __init__(self, ctx, config):
        self.ctx = ctx
        self.training_params = TrainingParams.from_strategy_config(config)
        self.candles = []

    def is_valid(self):
        return self.training_params.is_valid()

    def on_candle(self, candle):
        self.candles.append(candle)

    def on_finish(self):
        paths = markup_files(MARKUP_DIR)
        dataset = load_markup_dataset(paths)
        if dataset is None or len(dataset.y) < (self.training_params.folds + 1) * self.training_params.min_data_in_leaf:
            print(f"RegimeClassifierTraining: not enough rows in {MARKUP_DIR}/*.csv", flush=True)
            return
        print(f"RegimeClassifierTraining: {len(dataset.y)} rows from {len(paths)} markup files", flush=True)

        _, reports = model.walk_forward(dataset, self.training_params)
        for fold, report in enumerate(reports):
            print(f"RegimeClassifierTraining fold {fold}: {report}", flush=True)

        final_model = model.train(dataset.x, dataset.y, self.training_params)
        self.save(final_model, dataset, paths, reports)
        self.render_predictions(final_model)

    def render_predictions(self, final_model):
        if len(self.candles) <= features.max_window:
            return

        feature_builder = features.FeatureBuilder(self.candles)
        candle_indexes = range(features.max_window, len(self.candles))
        probabilities = final_model.predict([feature_builder.build(i) for i in candle_indexes])

        for i, candle_probabilities in zip(candle_indexes, probabilities):
            ts_ms = self.candles[i].ts_ms
            self.ctx.push_market_state(ts_ms, TO_MARKET_STATE[labeling.LABELS[candle_probabilities.argmax()]])
	    # overloads the chart
            # for label, probability in zip(labeling.LABELS, candle_probabilities):
            #     self.ctx.plot(ts_ms, "regime_model", label, probability)

    def save(self, final_model, dataset, paths, reports):
        os.makedirs(MODELS_DIR, exist_ok=True)
        path = os.path.join(MODELS_DIR, f"RegimeModel_{dataset.ts_ms[0]}_{dataset.ts_ms[-1]}")

        final_model.save_model(path + ".txt")
        with open(path + ".json", "w") as f:
            json.dump(
                {
                    "labels": labeling.LABELS,
                    "features": features.feature_names(),
                    "training_params": asdict(self.training_params),
                    "markup_files": [os.path.basename(p) for p in paths],
                    "walk_forward": [asdict(r) for r in reports],
                },
                f,
                indent=4,
            )
        print(f"RegimeClassifierTraining: model saved to {path}.txt", flush=True)
