"""
LightGBM regime classifier and its walk-forward validation.
Doesn't depend on the app, can be used from a notebook.
"""

import glob
import json
import os
from dataclasses import dataclass

import lightgbm
import numpy as np
from sklearn.metrics import accuracy_score, f1_score

from regime import features, labeling


@dataclass
class TrainingParams:
    num_boost_round: int
    num_leaves: int
    learning_rate: float
    min_data_in_leaf: int
    folds: int

    @staticmethod
    def from_strategy_config(config):
        return TrainingParams(
            num_boost_round=int(config["num_boost_round"]),
            num_leaves=int(config["num_leaves"]),
            learning_rate=config["learning_rate"],
            min_data_in_leaf=int(config["min_data_in_leaf"]),
            folds=int(config["folds"]),
        )

    def is_valid(self):
        return self.num_boost_round > 0 and self.num_leaves > 1 and self.learning_rate > 0 and self.folds > 0

    def lightgbm_params(self):
        return {
            "objective": "multiclass",
            "num_class": len(labeling.LABELS),
            "learning_rate": self.learning_rate,
            "num_leaves": self.num_leaves,
            "min_data_in_leaf": self.min_data_in_leaf,
            "feature_fraction": 0.8,
            "bagging_fraction": 0.8,
            "bagging_freq": 1,
            "lambda_l2": 1.0,
            # Reproducible results
            "seed": 0,
            "deterministic": True,
            "num_threads": 1,
            "verbose": -1,
        }


@dataclass
class FoldReport:
    train_rows: int
    test_rows: int
    accuracy: float
    macro_f1: float
    f1_per_label: dict

    def __str__(self):
        per_label = " ".join(f"{label}={f1:.2f}" for label, f1 in self.f1_per_label.items())
        return f"train={self.train_rows} test={self.test_rows} accuracy={self.accuracy:.3f} macro_f1={self.macro_f1:.3f} | f1: {per_label}"


def balanced_weights(y):
    """Rare labels (HIGH_VOLATILITY) get the same total weight as frequent ones."""
    counts = np.bincount(y, minlength=len(labeling.LABELS))
    label_weights = len(y) / (len(labeling.LABELS) * np.maximum(counts, 1))
    return label_weights[y]


def train(x, y, params):
    data = lightgbm.Dataset(x, label=y, weight=balanced_weights(y), feature_name=features.feature_names())
    return lightgbm.train(params.lightgbm_params(), data, num_boost_round=params.num_boost_round)


def evaluate(y_true, y_pred, train_rows):
    all_labels = list(range(len(labeling.LABELS)))
    f1_per_label = f1_score(y_true, y_pred, labels=all_labels, average=None, zero_division=0)
    return FoldReport(
        train_rows=train_rows,
        test_rows=len(y_true),
        accuracy=accuracy_score(y_true, y_pred),
        macro_f1=f1_score(y_true, y_pred, labels=all_labels, average="macro", zero_division=0),
        f1_per_label=dict(zip(labeling.LABELS, f1_per_label)),
    )


def walk_forward(dataset, params):
    """
    Splits rows into folds + 1 time chunks. Fold k trains on chunks [0, k] and tests on chunk k + 1.
    Train rows whose labels are known only at or after the test start are dropped: they were made from test prices.

    Returns out-of-sample probabilities (NaN for the first chunk, it's never tested) and a report per fold.
    """
    rows = len(dataset.y)
    chunk_bounds = np.linspace(0, rows, params.folds + 2).astype(int)
    probabilities = np.full((rows, len(labeling.LABELS)), np.nan)
    reports = []

    for fold in range(params.folds):
        test_begin, test_end = chunk_bounds[fold + 1], chunk_bounds[fold + 2]
        train_rows = np.flatnonzero(dataset.label_known_at_ts_ms[:test_begin] < dataset.ts_ms[test_begin])

        model = train(dataset.x[train_rows], dataset.y[train_rows], params)
        fold_probabilities = model.predict(dataset.x[test_begin:test_end])
        probabilities[test_begin:test_end] = fold_probabilities

        reports.append(evaluate(dataset.y[test_begin:test_end], fold_probabilities.argmax(axis=1), len(train_rows)))

    return probabilities, reports


def latest_model_path(models_dir):
    """The most recently saved model, without extension. None if there are no models."""
    paths = glob.glob(os.path.join(models_dir, "RegimeModel_*.txt"))
    if not paths:
        return None
    return os.path.splitext(max(paths, key=os.path.getmtime))[0]


def load_model(path):
    """Loads <path>.txt, checks that <path>.json matches current labels and features."""
    with open(path + ".json") as f:
        meta = json.load(f)
    if tuple(meta["labels"]) != labeling.LABELS:
        raise ValueError(f"{path}: labels {meta['labels']} differ from {labeling.LABELS}, retrain the model")
    if meta["features"] != features.feature_names():
        raise ValueError(f"{path}: features differ from regime/features.py, retrain the model")
    return lightgbm.Booster(model_file=path + ".txt")
