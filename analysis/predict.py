"""Generate seven-day demand and next-24-hour peak/idle predictions."""

from __future__ import annotations

import argparse
import csv
import json
import math
import pickle
from datetime import datetime, timedelta
from pathlib import Path

from feature_engineering import atomic_write_csv


def _load_pickle(path: Path) -> dict:
    with path.open("rb") as handle:
        model = pickle.load(handle)
    if not isinstance(model, dict) or "kind" not in model:
        raise ValueError(f"invalid model artifact: {path}")
    return model


def _read_station_hourly(path: Path) -> list[dict]:
    with path.open(encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def _daily_value(model: dict, moment: datetime, step: int) -> float:
    base = float(model["weekday_means"].get(str(moment.weekday()), model["global_mean"]))
    return max(0.0, base + float(model.get("trend_per_day", 0.0)) * min(step, 7))


def _hourly_value(model: dict, moment: datetime) -> float:
    key = f"{moment.weekday()}-{moment.hour}"
    return max(0.0, float(model["weekday_hour_means"].get(
        key, model["hour_means"].get(str(moment.hour), model["global_mean"]))))


def generate_predictions(output_root: Path, now: datetime | None = None) -> dict[str, int]:
    models = output_root / "models"
    daily_model = _load_pickle(models / "demand_forecast.pkl")
    hourly_model = _load_pickle(models / "peak_predict.pkl")
    current = now.astimezone() if now is not None else datetime.now().astimezone()
    predictions = output_root / "predictions"

    daily_sigma = max(0.0, float(daily_model.get("residual_std", 0.0)))
    daily_rows: list[dict] = []
    first_day = current.date() + timedelta(days=1)
    for step in range(1, 8):
        day = first_day + timedelta(days=step - 1)
        moment = datetime.combine(day, datetime.min.time())
        value = _daily_value(daily_model, moment, step)
        daily_rows.append({
            "date": day.isoformat(),
            "predicted_kwh": round(value, 4),
            "lower_bound": round(max(0.0, value - 1.96 * daily_sigma), 4),
            "upper_bound": round(value + 1.96 * daily_sigma, 4),
            "confidence": 0.95,
            "model_version": daily_model.get("version", "unknown"),
            "simulated_data": 1,
        })
    atomic_write_csv(predictions / "demand_next7.csv", list(daily_rows[0]), daily_rows)

    first_hour = current.replace(minute=0, second=0, microsecond=0) + timedelta(hours=1)
    hourly_sigma = max(0.0, float(hourly_model.get("residual_std", 0.0)))
    provisional = [
        (first_hour + timedelta(hours=offset),
         _hourly_value(hourly_model, first_hour + timedelta(hours=offset)))
        for offset in range(24)
    ]
    mean_prediction = sum(value for _, value in provisional) / len(provisional)
    hourly_rows: list[dict] = []
    for stamp, value in provisional:
        if value > mean_prediction * 1.5:
            level = "peak"
        elif value < mean_prediction * 0.8:
            level = "valley"
        else:
            level = "flat"
        hourly_rows.append({
            "timestamp": stamp.isoformat(),
            "hour": stamp.hour,
            "predicted_kwh": round(value, 4),
            "peak_level": level,
            "lower_bound": round(max(0.0, value - 1.96 * hourly_sigma), 4),
            "upper_bound": round(value + 1.96 * hourly_sigma, 4),
            "model_version": hourly_model.get("version", "unknown"),
            "simulated_data": 1,
        })
    atomic_write_csv(predictions / "peak_hours.csv", list(hourly_rows[0]), hourly_rows)

    station_rows = _read_station_hourly(
        output_root / "data" / "processed" / "station_hourly.csv")
    by_station: dict[tuple[int, str, int], float] = {}
    station_meta: dict[int, tuple[str, int]] = {}
    for row in station_rows:
        station_id = int(row["station_id"])
        station_meta[station_id] = (row["station_name"], int(row["pile_count"]))
        by_station[(station_id, row["station_name"], int(row["hour"]))] = float(
            row["avg_sessions_per_day"])
    historical_hour_mean = {hour: max(0.000001, float(value))
                            for hour, value in ((int(key), val)
                                                for key, val in hourly_model["hour_means"].items())}
    idle_rows: list[dict] = []
    for station_id, (station_name, pile_count) in sorted(station_meta.items()):
        row: dict[str, object] = {
            "station_id": station_id,
            "station_name": station_name,
            "start_time": first_hour.isoformat(),
            "pile_count": pile_count,
            "simulated_data": 1,
        }
        for offset, (stamp, predicted_kwh) in enumerate(provisional):
            sessions = by_station.get((station_id, station_name, stamp.hour), 0.0)
            base_utilization = min(1.0, sessions / max(1, pile_count))
            load_factor = predicted_kwh / historical_hour_mean.get(stamp.hour, 0.000001)
            utilization = min(1.0, max(0.0, base_utilization * load_factor))
            row[f"slot_{offset}"] = max(0, min(pile_count,
                int(round(pile_count * (1.0 - utilization)))))
        idle_rows.append(row)
    if idle_rows:
        atomic_write_csv(predictions / "idle_predict.csv", list(idle_rows[0]), idle_rows)
    return {"daily_predictions": len(daily_rows), "hourly_predictions": len(hourly_rows),
            "station_predictions": len(idle_rows)}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path("analysis"))
    args = parser.parse_args()
    print(json.dumps(generate_predictions(args.output), ensure_ascii=False))


if __name__ == "__main__":
    main()
