"""Train, validate, version, and publish daily/hourly forecasting models."""

from __future__ import annotations

import argparse
import csv
import json
import math
import os
import pickle
import statistics
import tempfile
import warnings
from datetime import datetime, timezone
from pathlib import Path
from typing import Sequence

try:
    from statsmodels.tsa.arima.model import ARIMA
except ImportError:  # The pipeline remains usable with an explicitly labelled SMA fallback.
    ARIMA = None


def _read_partitioned_series(path: Path, time_field: str) -> dict[str, list[tuple[str, float]]]:
    partitions: dict[str, list[tuple[str, float]]] = {
        "train": [], "validation": [], "test": []
    }
    with path.open(encoding="utf-8", newline="") as handle:
        for row in csv.DictReader(handle):
            split = row.get("split", "train")
            if split not in partitions:
                raise ValueError(f"invalid split {split!r} in {path}")
            partitions[split].append((row[time_field], float(row["kwh"])))
    return partitions


def _mean(values: Sequence[float]) -> float:
    return statistics.fmean(values) if values else 0.0


def _metrics(actual: Sequence[float], predicted: Sequence[float]) -> dict[str, float | None]:
    if not actual:
        return {"mae": None, "rmse": None, "mape": None}
    if len(actual) != len(predicted):
        raise ValueError("actual and predicted lengths differ")
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
    return (statistics.pstdev(residuals) if len(residuals) > 1
            else max(1.0, _mean(actual) * 0.15))


def _sma_value(series: Sequence[tuple[str, float]], window: int) -> float:
    values = [value for _, value in series]
    return _mean(values[-window:])


def _hour_means(series: Sequence[tuple[str, float]]) -> dict[str, float]:
    values: dict[int, list[float]] = {hour: [] for hour in range(24)}
    all_values = [value for _, value in series]
    global_mean = _mean(all_values)
    for stamp, value in series:
        values[datetime.fromisoformat(stamp).hour].append(value)
    return {str(hour): _mean(items) if items else global_mean
            for hour, items in values.items()}


def _sma_artifact(name: str, series: Sequence[tuple[str, float]], window: int,
                  generated_at: str, version: str) -> dict:
    if not series:
        raise ValueError(f"cannot build {name} SMA without observations")
    value = _sma_value(series, window)
    history = [item for _, item in series]
    fitted = [_mean(history[max(0, index - window):index])
              for index in range(1, len(history))]
    actual = history[1:]
    artifact = {
        "kind": "sma",
        "algorithm": f"simple moving average (window={window})",
        "window": window,
        "value": value,
        "residual_std": _residual_std(actual, fitted),
        "data_through": series[-1][0],
        "point_count": len(series),
        "generated_at": generated_at,
        "version": version,
        "simulated_data": True,
    }
    if name == "peak_predict":
        artifact["hour_means"] = _hour_means(series)
    return artifact


def _fit_arima(values: Sequence[float], order: tuple[int, int, int]):
    if ARIMA is None:
        raise RuntimeError("statsmodels is not installed")
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        return ARIMA(list(values), order=order,
                     enforce_stationarity=False,
                     enforce_invertibility=False).fit()


def _arima_artifact(name: str, series: Sequence[tuple[str, float]],
                    order: tuple[int, int, int], generated_at: str,
                    version: str) -> dict:
    estimator = _fit_arima([value for _, value in series], order)
    residuals = [float(value) for value in estimator.resid]
    residual_std = (statistics.pstdev(residuals) if len(residuals) > 1
                    else max(1.0, _mean([value for _, value in series]) * 0.15))
    artifact = {
        "kind": "arima",
        "algorithm": f"ARIMA{order}",
        "order": order,
        "estimator": estimator,
        "residual_std": residual_std,
        "data_through": series[-1][0],
        "point_count": len(series),
        "generated_at": generated_at,
        "version": version,
        "simulated_data": True,
    }
    if name == "peak_predict":
        artifact["hour_means"] = _hour_means(series)
    return artifact


def _forecast_values(artifact: dict, steps: int) -> list[float]:
    if steps <= 0:
        return []
    if artifact["kind"] == "arima":
        return [max(0.0, float(value))
                for value in artifact["estimator"].forecast(steps=steps)]
    if artifact["kind"] == "sma":
        return [max(0.0, float(artifact["value"]))] * steps
    raise ValueError(f"unsupported model kind: {artifact['kind']}")


def _candidate_accepted(candidate: dict[str, float | None],
                        baseline: dict[str, float | None]) -> bool:
    candidate_rmse = candidate["rmse"]
    baseline_rmse = baseline["rmse"]
    return (candidate_rmse is not None
            and (baseline_rmse is None or candidate_rmse <= baseline_rmse * 1.10))


def _build_model(name: str, partitions: dict[str, list[tuple[str, float]]],
                 generated_at: str, version: str) -> tuple[dict, dict]:
    train = partitions["train"]
    validation = partitions["validation"]
    test = partitions["test"]
    if not train:
        raise ValueError(f"{name} has no training observations")

    is_daily = name == "demand_forecast"
    order = (1, 1, 1) if is_daily else (2, 1, 1)
    minimum_points = 14 if is_daily else 72
    window = 7 if is_daily else 24
    baseline_predictions = [_sma_value(train, window)] * len(validation)
    baseline_metrics = _metrics([value for _, value in validation], baseline_predictions)
    candidate_metrics = {"mae": None, "rmse": None, "mape": None}
    candidate_error = ""
    accepted = False

    if not validation:
        candidate_error = "validation partition is empty"
    elif len(train) < minimum_points:
        candidate_error = (f"insufficient training data: {len(train)} points; "
                           f"ARIMA requires at least {minimum_points}")
    elif ARIMA is None:
        candidate_error = "statsmodels is not installed; using labelled SMA fallback"
    else:
        try:
            validation_estimator = _fit_arima([value for _, value in train], order)
            candidate_predictions = [max(0.0, float(value)) for value in
                                     validation_estimator.forecast(steps=len(validation))]
            candidate_metrics = _metrics([value for _, value in validation],
                                         candidate_predictions)
            accepted = _candidate_accepted(candidate_metrics, baseline_metrics)
            if not accepted:
                candidate_error = "candidate validation RMSE exceeds baseline tolerance"
        except Exception as error:  # Fit failures must not destroy the last active model.
            candidate_error = f"ARIMA fit failed: {error}"

    development = train + validation
    if accepted:
        selected = _arima_artifact(name, development, order, generated_at, version)
    else:
        selected = _sma_artifact(name, development, window, generated_at, version)

    test_predictions = _forecast_values(selected, len(test))
    report = {
        "candidate_accepted": accepted,
        "candidate_error": candidate_error,
        "candidate_algorithm": f"ARIMA{order}",
        "selected_algorithm": selected["algorithm"],
        "source_features": ("train_demand.csv" if is_daily else "train_peak.csv"),
        "partition_counts": {key: len(value) for key, value in partitions.items()},
        "validation_metrics": {
            "candidate": candidate_metrics,
            "baseline": baseline_metrics,
        },
        "test_metrics": _metrics([value for _, value in test], test_predictions),
    }
    return selected, report


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


def _model_summary(model: dict) -> dict:
    return {key: value for key, value in model.items()
            if key not in {"estimator", "hour_means"}}


def _publish_model(models: Path, name: str, selected: dict, report: dict,
                   version: str) -> dict:
    versions = models / "versions"
    candidate_path = versions / f"{name}-{version}.pkl"
    payload = pickle.dumps(selected, protocol=pickle.HIGHEST_PROTOCOL)
    _atomic_bytes(candidate_path, payload)
    active_path = models / f"{name}.pkl"
    active_updated = report["candidate_accepted"] or not active_path.exists()
    if active_updated:
        _atomic_bytes(active_path, payload)
    with active_path.open("rb") as handle:
        active_model = pickle.load(handle)
    old_versions = sorted(versions.glob(f"{name}-*.pkl"), reverse=True)
    for stale in old_versions[3:]:
        stale.unlink()
    return {
        "accepted": report["candidate_accepted"],
        "active_updated": active_updated,
        "active_model": _model_summary(active_model),
        "active_path": str(active_path),
        "candidate_path": str(candidate_path),
    }


def train_models(output_root: Path) -> dict:
    processed = output_root / "data" / "processed"
    daily = _read_partitioned_series(processed / "train_demand.csv", "date")
    hourly = _read_partitioned_series(processed / "train_peak.csv", "timestamp")
    if not daily["train"] or not hourly["train"]:
        raise ValueError("processed daily/hourly training data is empty")
    generated_at = datetime.now(timezone.utc).isoformat()
    version = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    daily_model, daily_report = _build_model(
        "demand_forecast", daily, generated_at, version)
    hourly_model, hourly_report = _build_model(
        "peak_predict", hourly, generated_at, version)
    models = output_root / "models"
    daily_publish = _publish_model(models, "demand_forecast", daily_model,
                                   daily_report, version)
    hourly_publish = _publish_model(models, "peak_predict", hourly_model,
                                    hourly_report, version)
    metadata = {
        "generated_at": generated_at,
        "simulated_data": True,
        "models": {
            "demand_forecast": {**daily_report, **daily_publish},
            "peak_predict": {**hourly_report, **hourly_publish},
        },
    }
    _atomic_json(models / "model_meta.json", metadata)
    return metadata


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path("analysis"))
    args = parser.parse_args()
    result = train_models(args.output)
    print(json.dumps({name: {
        "candidate_accepted": details["accepted"],
        "active_updated": details["active_updated"],
        "active_algorithm": details["active_model"]["algorithm"],
    } for name, details in result["models"].items()}, ensure_ascii=False))


if __name__ == "__main__":
    main()
