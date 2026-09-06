"""Train versioned, explainable daily and hourly seasonal models."""

from __future__ import annotations

import argparse
import csv
import json
import math
import os
import pickle
import statistics
import tempfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Sequence


def _read_series(path: Path, time_field: str) -> list[tuple[str, float]]:
    with path.open(encoding="utf-8", newline="") as handle:
        return [(row[time_field], float(row["kwh"])) for row in csv.DictReader(handle)]


def _mean(values: Sequence[float]) -> float:
    return statistics.fmean(values) if values else 0.0


def _linear_slope(values: Sequence[float]) -> float:
    if len(values) < 2:
        return 0.0
    x_mean = (len(values) - 1) / 2.0
    y_mean = _mean(values)
    denominator = sum((index - x_mean) ** 2 for index in range(len(values)))
    return (sum((index - x_mean) * (value - y_mean)
                for index, value in enumerate(values)) / denominator
            if denominator else 0.0)


def _metrics(actual: Sequence[float], predicted: Sequence[float]) -> dict[str, float | None]:
    if not actual:
        return {"mae": None, "rmse": None, "mape": None}
    errors = [prediction - observed for observed, prediction in zip(actual, predicted)]
    nonzero = [(observed, prediction) for observed, prediction in zip(actual, predicted)
               if observed != 0]
    return {
        "mae": round(_mean([abs(error) for error in errors]), 6),
        "rmse": round(math.sqrt(_mean([error * error for error in errors])), 6),
        "mape": (round(_mean([abs(prediction - observed) / abs(observed)
                              for observed, prediction in nonzero]), 6)
                 if nonzero else None),
    }


def _residual_std(actual: Sequence[float], predicted: Sequence[float]) -> float:
    residuals = [observed - prediction for observed, prediction in zip(actual, predicted)]
    return statistics.pstdev(residuals) if len(residuals) > 1 else max(1.0, _mean(actual) * 0.15)


def _daily_model(series: list[tuple[str, float]]) -> tuple[dict, dict]:
    train_count = max(1, int(len(series) * 0.8))
    train = series[:train_count]
    validation = series[train_count:] or series[-1:]
    values = [value for _, value in train]
    global_mean = _mean(values)
    weekday_values: dict[int, list[float]] = {weekday: [] for weekday in range(7)}
    for stamp, value in train:
        weekday_values[datetime.fromisoformat(stamp).weekday()].append(value)
    weekday_means = {str(key): _mean(items) if items else global_mean
                     for key, items in weekday_values.items()}
    slope = _linear_slope(values[-30:])
    candidate = []
    baseline = []
    actual = []
    for offset, (stamp, value) in enumerate(validation, start=1):
        weekday = datetime.fromisoformat(stamp).weekday()
        candidate.append(max(0.0, weekday_means[str(weekday)] + slope * min(offset, 7)))
        baseline.append(global_mean)
        actual.append(value)
    candidate_metrics = _metrics(actual, candidate)
    baseline_metrics = _metrics(actual, baseline)
    model = {
        "kind": "daily_seasonal_trend",
        "algorithm": "weekday seasonal mean plus capped linear trend",
        "weekday_means": weekday_means,
        "global_mean": global_mean,
        "trend_per_day": slope,
        "residual_std": _residual_std(actual, candidate),
        "data_through": series[-1][0],
        "point_count": len(series),
    }
    return model, {"candidate": candidate_metrics, "baseline": baseline_metrics}


def _hourly_model(series: list[tuple[str, float]]) -> tuple[dict, dict]:
    train_count = max(1, int(len(series) * 0.8))
    train = series[:train_count]
    validation = series[train_count:] or series[-1:]
    values = [value for _, value in train]
    global_mean = _mean(values)
    hour_values: dict[int, list[float]] = {hour: [] for hour in range(24)}
    weekday_hour_values: dict[str, list[float]] = {}
    for stamp, value in train:
        moment = datetime.fromisoformat(stamp)
        hour_values[moment.hour].append(value)
        weekday_hour_values.setdefault(f"{moment.weekday()}-{moment.hour}", []).append(value)
    hour_means = {str(hour): _mean(items) if items else global_mean
                  for hour, items in hour_values.items()}
    weekday_hour_means = {key: _mean(items) for key, items in weekday_hour_values.items()}
    actual: list[float] = []
    candidate: list[float] = []
    baseline: list[float] = []
    for stamp, value in validation:
        moment = datetime.fromisoformat(stamp)
        candidate.append(weekday_hour_means.get(
            f"{moment.weekday()}-{moment.hour}", hour_means[str(moment.hour)]))
        baseline.append(global_mean)
        actual.append(value)
    model = {
        "kind": "hourly_seasonal",
        "algorithm": "weekday-hour seasonal mean with hour fallback",
        "hour_means": hour_means,
        "weekday_hour_means": weekday_hour_means,
        "global_mean": global_mean,
        "residual_std": _residual_std(actual, candidate),
        "data_through": series[-1][0],
        "point_count": len(series),
    }
    return model, {"candidate": _metrics(actual, candidate),
                   "baseline": _metrics(actual, baseline)}


def _atomic_bytes(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temp_name = tempfile.mkstemp(prefix=f".{path.name}.", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as handle:
            handle.write(data)
        os.replace(temp_name, path)
    except Exception:
        try:
            os.unlink(temp_name)
        except FileNotFoundError:
            pass
        raise


def _atomic_json(path: Path, value: dict) -> None:
    _atomic_bytes(path, (json.dumps(value, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))


def _accepted(metrics: dict) -> bool:
    candidate = metrics["candidate"]["rmse"]
    baseline = metrics["baseline"]["rmse"]
    return candidate is not None and (baseline is None or candidate <= baseline * 1.10)


def _publish_model(models: Path, name: str, model: dict, metrics: dict,
                   version: str) -> dict:
    versions = models / "versions"
    candidate_path = versions / f"{name}-{version}.pkl"
    payload = pickle.dumps(model, protocol=pickle.HIGHEST_PROTOCOL)
    _atomic_bytes(candidate_path, payload)
    active_path = models / f"{name}.pkl"
    accepted = _accepted(metrics) or not active_path.exists()
    if accepted:
        _atomic_bytes(active_path, payload)
    old_versions = sorted(versions.glob(f"{name}-*.pkl"), reverse=True)
    for stale in old_versions[3:]:
        stale.unlink()
    return {"accepted": accepted, "active_path": str(active_path),
            "candidate_path": str(candidate_path)}


def train_models(output_root: Path) -> dict:
    processed = output_root / "data" / "processed"
    daily = _read_series(processed / "daily_kwh.csv", "date")
    hourly = _read_series(processed / "hourly_kwh.csv", "timestamp")
    if not daily or not hourly:
        raise ValueError("processed daily/hourly data is empty")
    daily_model, daily_metrics = _daily_model(daily)
    hourly_model, hourly_metrics = _hourly_model(hourly)
    generated_at = datetime.now(timezone.utc).isoformat()
    version = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    daily_model.update({"generated_at": generated_at, "version": version,
                        "simulated_data": True})
    hourly_model.update({"generated_at": generated_at, "version": version,
                         "simulated_data": True})
    models = output_root / "models"
    daily_publish = _publish_model(models, "demand_forecast", daily_model,
                                   daily_metrics, version)
    hourly_publish = _publish_model(models, "peak_predict", hourly_model,
                                    hourly_metrics, version)
    metadata = {
        "generated_at": generated_at,
        "simulated_data": True,
        "models": {
            "demand_forecast": {**daily_model, "metrics": daily_metrics,
                                **daily_publish},
            "peak_predict": {**hourly_model, "metrics": hourly_metrics,
                             **hourly_publish},
        },
    }
    _atomic_json(models / "model_meta.json", metadata)
    return metadata


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path("analysis"))
    args = parser.parse_args()
    result = train_models(args.output)
    print(json.dumps({name: details["accepted"]
                      for name, details in result["models"].items()}, ensure_ascii=False))


if __name__ == "__main__":
    main()
