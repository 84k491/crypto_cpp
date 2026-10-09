# RegimeClassifierTraining

Trains a LightGBM model that predicts market regime: `NO_TREND`, `UP_TREND`, `DOWN_TREND`, `HIGH_VOLATILITY`.
It doesn't trade.

## Requirements

`lightgbm`, `numpy`, `scikit-learn` installed for the system Python 3.10:

```fish
python3 -m pip install --user lightgbm numpy scikit-learn
```

## 1. Make markup files

Run `RegimeMarkup` backtests on the periods you want to train on.
Each run writes `markup/RegimeMarkup_<first_ts>_<last_ts>.csv` with features and labels.

- Use the same `timeframe_s` for all files.
- Use several months of data, in one or several files. One week is not enough.
- Leave out the period you want to test the model on.

## 2. Train

Run a `RegimeClassifierTraining` backtest on a period that is **not** in the markup files.
The training happens when the run finishes:

1. Loads all `markup/*.csv`.
2. Walk-forward validation, prints a report per fold to stdout:
   `accuracy`, `macro_f1` and F1 per label. Random guess gives macro F1 ≈ 0.25.
3. Trains the final model on all rows and saves:
   - `models/RegimeModel_<first_ts>_<last_ts>.txt`: the LightGBM model
   - `models/RegimeModel_<first_ts>_<last_ts>.json`: labels order, features, parameters, markup files, validation reports
4. Runs the model on the run's candles:
   - `regime_model` chart: probability of every label, they sum to 1
   - price chart: the label with the highest probability. It is not a decision: a low-confidence winner
     (e.g. 0.35 vs 0.30) looks the same as a confident one. Look at the probabilities

## Parameters

| Parameter | Default | Meaning |
|---|---|---|
| `timeframe_s` | 60 | Candle size. Must match the markup files |
| `num_boost_round` | 300 | Number of trees |
| `num_leaves` | 15 | Tree complexity. Higher fits more, overfits easier |
| `learning_rate` | 0.05 | Step of each tree. Lower needs more trees |
| `min_data_in_leaf` | 100 | Min rows in a leaf. Higher is more robust |
| `folds` | 4 | Number of walk-forward folds |

## Notes

- Results are reproducible: same files and parameters give the same model.
- Rare labels (`HIGH_VOLATILITY`) get higher weight in training.
- Markup labels look into the future. Validation drops train rows whose labels depend on test prices.
- After changing `regime/features.py`, re-run `RegimeMarkup`: old files have old feature columns.
