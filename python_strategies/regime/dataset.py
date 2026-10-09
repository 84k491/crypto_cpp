import csv
import glob
import os
from dataclasses import dataclass

import numpy as np

from regime import features, labeling


@dataclass
class Dataset:
    """One row per labeled candle, rows are in time order."""

    ts_ms: np.ndarray
    x: np.ndarray
    y: np.ndarray  # index in labeling.LABELS
    # Labels look into the future: the row's label is made from prices up to this timestamp
    label_known_at_ts_ms: np.ndarray


def markup_files(markup_dir):
    return sorted(glob.glob(os.path.join(markup_dir, "*.csv")))


def label_known_at(ts_ms, labels):
    """
    End of the run of equal labels that the row belongs to: the first row after the run.
    A run consists of whole swings, so it never ends before the row's swing.
    The last run of a file is known at its last row.
    """
    known_at = [0] * len(labels)
    run_end_ts = ts_ms[-1]
    for row in range(len(labels) - 1, -1, -1):
        if row + 1 < len(labels) and labels[row + 1] != labels[row]:
            run_end_ts = ts_ms[row + 1]
        known_at[row] = run_end_ts
    return known_at


def read_markup_file(path):
    with open(path, newline="") as f:
        rows = list(csv.DictReader(f))
    if not rows:
        return None

    missing = [name for name in features.feature_names() + ["ts_ms", "label"] if name not in rows[0]]
    if missing:
        raise ValueError(f"{path} has no columns {missing}, re-run RegimeMarkup")

    ts_ms = [int(r["ts_ms"]) for r in rows]
    labels = [r["label"] for r in rows]
    return Dataset(
        ts_ms=np.array(ts_ms, dtype=np.int64),
        x=np.array([[float(r[name]) for name in features.feature_names()] for r in rows], dtype=np.float64),
        y=np.array([labeling.LABELS.index(label) for label in labels], dtype=np.int64),
        label_known_at_ts_ms=np.array(label_known_at(ts_ms, labels), dtype=np.int64),
    )


def load_markup_dataset(paths):
    parts = [d for d in (read_markup_file(p) for p in paths) if d is not None]
    if not parts:
        return None

    ts_ms = np.concatenate([d.ts_ms for d in parts])
    order = np.argsort(ts_ms, kind="stable")
    return Dataset(
        ts_ms=ts_ms[order],
        x=np.concatenate([d.x for d in parts])[order],
        y=np.concatenate([d.y for d in parts])[order],
        label_known_at_ts_ms=np.concatenate([d.label_known_at_ts_ms for d in parts])[order],
    )
