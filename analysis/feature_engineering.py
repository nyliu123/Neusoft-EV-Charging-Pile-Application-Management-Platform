"""Build leakage-safe daily/hourly features from the read-only demo database."""

from __future__ import annotations

import argparse
import csv
import math
import os
import sqlite3
import statistics
import tempfile
from collections import defaultdict
from datetime import date, datetime, timedelta
from pathlib import Path
from typing import Iterable, Sequence


def atomic_write_csv(path: Path, fieldnames: Sequence[str], rows: Iterable[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temp_name = tempfile.mkstemp(prefix=f".{path.name}.", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(rows)
        os.replace(temp_name, path)
    except Exception:
        try:
            os.unlink(temp_name)
        except FileNotFoundError:
            pass
        raise


def _split_label(index: int, count: int) -> str:
    if count < 3:
        return "train"
    train_end = min(max(1, int(count * 0.8)), count - 2)
    validation_end = max(train_end + 1, int(count * 0.9))
    validation_end = min(validation_end, count - 1)
    if index < train_end:
        return "train"
    if index < validation_end:
        return "validation"
    return "test"


def _mean(values: Sequence[float]) -> float | None:
    return sum(values) / len(values) if values else None


def _standard_deviation(values: Sequence[float]) -> float | None:
    return statistics.pstdev(values) if len(values) > 1 else None


def _linear_slope(values: Sequence[float]) -> float | None:
    if len(values) < 2:
        return None
    x_mean = (len(values) - 1) / 2.0
    y_mean = sum(values) / len(values)
    denominator = sum((index - x_mean) ** 2 for index in range(len(values)))
    if denominator == 0:
        return None
    return sum((index - x_mean) * (value - y_mean)
               for index, value in enumerate(values)) / denominator


def _daily_series(connection: sqlite3.Connection) -> list[tuple[date, float]]:
    rows = connection.execute(
        """
        SELECT date(end_time), COALESCE(SUM(charge_amount_kwh), 0)
        FROM orders
        WHERE status = 'settled' AND end_time IS NOT NULL
        GROUP BY date(end_time)
        ORDER BY date(end_time)
        """
    ).fetchall()
    if not rows:
        return []
    totals = {date.fromisoformat(day): float(value) for day, value in rows}
    current = min(totals)
    last = max(totals)
    result: list[tuple[date, float]] = []
    while current <= last:
        result.append((current, totals.get(current, 0.0)))
        current += timedelta(days=1)
    return result


def _hourly_series(connection: sqlite3.Connection) -> list[tuple[datetime, float]]:
    rows = connection.execute(
        """
        SELECT strftime('%Y-%m-%dT%H:00:00', end_time),
               COALESCE(SUM(charge_amount_kwh), 0)
        FROM orders
        WHERE status = 'settled' AND end_time IS NOT NULL
        GROUP BY strftime('%Y-%m-%dT%H:00:00', end_time)
        ORDER BY strftime('%Y-%m-%dT%H:00:00', end_time)
        """
    ).fetchall()
    if not rows:
        return []
    totals = {datetime.fromisoformat(stamp): float(value) for stamp, value in rows}
    current = min(totals)
    last = max(totals)
    result: list[tuple[datetime, float]] = []
    while current <= last:
        result.append((current, totals.get(current, 0.0)))
        current += timedelta(hours=1)
    return result


def _daily_features(series: Sequence[tuple[date, float]]) -> list[dict]:
    values = [value for _, value in series]
    rows: list[dict] = []
    for index, (day, value) in enumerate(series):
        history_7 = values[max(0, index - 7):index]
        history_14 = values[max(0, index - 14):index]
        std_7 = _standard_deviation(history_7)
        std_14 = _standard_deviation(history_14)
        trend_7 = _linear_slope(history_7)
        rows.append({
            "date": day.isoformat(),
            "kwh": round(value, 4),
            "day_of_week": day.weekday(),
            "month": day.month,
            "is_weekend": int(day.weekday() >= 5),
            "is_workday": int(day.weekday() < 5),
            "lag_1d": "" if index < 1 else round(values[index - 1], 4),
            "lag_2d": "" if index < 2 else round(values[index - 2], 4),
            "lag_3d": "" if index < 3 else round(values[index - 3], 4),
            "lag_7d": "" if index < 7 else round(values[index - 7], 4),
            "lag_14d": "" if index < 14 else round(values[index - 14], 4),
            "rolling_mean_7d": "" if not history_7 else round(_mean(history_7) or 0.0, 4),
            "rolling_std_7d": "" if std_7 is None else round(std_7, 4),
            "rolling_mean_14d": "" if not history_14 else round(_mean(history_14) or 0.0, 4),
            "rolling_std_14d": "" if std_14 is None else round(std_14, 4),
            "trend_7d": "" if trend_7 is None else round(trend_7, 6),
            "split": _split_label(index, len(series)),
        })
    return rows


def _hourly_features(series: Sequence[tuple[datetime, float]]) -> list[dict]:
    values = [value for _, value in series]
    rows: list[dict] = []
    for index, (stamp, value) in enumerate(series):
        history_24 = values[max(0, index - 24):index]
        history_168 = values[max(0, index - 168):index]
        angle = 2.0 * math.pi * stamp.hour / 24.0
        rows.append({
            "timestamp": stamp.isoformat(),
            "kwh": round(value, 4),
            "hour": stamp.hour,
            "hour_sin": round(math.sin(angle), 6),
            "hour_cos": round(math.cos(angle), 6),
            "day_of_week": stamp.weekday(),
            "is_weekend": int(stamp.weekday() >= 5),
            "lag_1h": "" if index < 1 else round(values[index - 1], 4),
            "lag_24h": "" if index < 24 else round(values[index - 24], 4),
            "lag_168h": "" if index < 168 else round(values[index - 168], 4),
            "rolling_mean_24h": "" if not history_24 else round(_mean(history_24) or 0.0, 4),
            "rolling_mean_168h": "" if not history_168 else round(_mean(history_168) or 0.0, 4),
            "split": _split_label(index, len(series)),
        })
    return rows


def _station_hourly(connection: sqlite3.Connection) -> list[dict]:
    stations = connection.execute(
        """
        SELECT s.station_id, s.station_name, COUNT(p.pile_id)
        FROM charging_stations s
        LEFT JOIN charging_piles p ON p.station_id = s.station_id
        GROUP BY s.station_id, s.station_name
        ORDER BY s.station_id
        """
    ).fetchall()
    observation_window = connection.execute(
        """
        SELECT MIN(date(end_time)), MAX(date(end_time))
        FROM orders
        WHERE status = 'settled' AND end_time IS NOT NULL
        """
    ).fetchone()
    observation_days = 1
    if observation_window and observation_window[0] and observation_window[1]:
        first_day = date.fromisoformat(observation_window[0])
        last_day = date.fromisoformat(observation_window[1])
        observation_days = (last_day - first_day).days + 1
    observations = connection.execute(
        """
        SELECT station_id, CAST(strftime('%H', end_time) AS INTEGER), COUNT(*)
        FROM orders
        WHERE status = 'settled' AND end_time IS NOT NULL
        GROUP BY station_id, CAST(strftime('%H', end_time) AS INTEGER)
        """
    ).fetchall()
    counts: dict[tuple[int, int], int] = {
        (int(station_id), int(hour)): int(sessions)
        for station_id, hour, sessions in observations
    }
    result: list[dict] = []
    for station_id, station_name, pile_count in stations:
        for hour in range(24):
            sessions = counts.get((int(station_id), hour), 0)
            result.append({
                "station_id": int(station_id),
                "station_name": station_name,
                "hour": hour,
                "pile_count": int(pile_count),
                "observation_days": observation_days,
                "avg_sessions_per_day": round(sessions / observation_days, 6),
            })
    return result


def build_features(database_path: Path, output_root: Path) -> dict[str, int]:
    if not database_path.exists():
        raise FileNotFoundError(f"database not found: {database_path}")
    connection = sqlite3.connect(f"file:{database_path.resolve()}?mode=ro", uri=True)
    try:
        daily = _daily_series(connection)
        hourly = _hourly_series(connection)
        stations = _station_hourly(connection)
    finally:
        connection.close()
    if not daily or not hourly:
        raise ValueError("at least one settled order with end_time is required")

    processed = output_root / "data" / "processed"
    daily_rows = _daily_features(daily)
    hourly_rows = _hourly_features(hourly)
    atomic_write_csv(processed / "daily_kwh.csv", ["date", "kwh"],
                     ({"date": day.isoformat(), "kwh": round(value, 4)} for day, value in daily))
    atomic_write_csv(processed / "hourly_kwh.csv", ["timestamp", "kwh"],
                     ({"timestamp": stamp.isoformat(), "kwh": round(value, 4)}
                      for stamp, value in hourly))
    atomic_write_csv(processed / "train_demand.csv", list(daily_rows[0]), daily_rows)
    atomic_write_csv(processed / "train_peak.csv", list(hourly_rows[0]), hourly_rows)
    atomic_write_csv(processed / "station_hourly.csv",
                     ["station_id", "station_name", "hour", "pile_count",
                      "observation_days", "avg_sessions_per_day"], stations)
    return {"daily_points": len(daily), "hourly_points": len(hourly),
            "station_hour_rows": len(stations)}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--database", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path("analysis"))
    args = parser.parse_args()
    print(build_features(args.database, args.output))


if __name__ == "__main__":
    main()
