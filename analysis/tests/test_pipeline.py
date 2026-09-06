from __future__ import annotations

import csv
import sqlite3
import sys
import tempfile
import unittest
from datetime import datetime, timedelta
from pathlib import Path


ANALYSIS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ANALYSIS_DIR))

from run_pipeline import run_pipeline  # noqa: E402


class PipelineTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.database = self.root / "demo.sqlite3"
        connection = sqlite3.connect(self.database)
        connection.executescript(
            """
            CREATE TABLE charging_stations (
                station_id INTEGER PRIMARY KEY,
                station_name TEXT NOT NULL
            );
            CREATE TABLE charging_piles (
                pile_id INTEGER PRIMARY KEY,
                station_id INTEGER NOT NULL
            );
            CREATE TABLE orders (
                order_id INTEGER PRIMARY KEY,
                station_id INTEGER NOT NULL,
                status TEXT NOT NULL,
                end_time TEXT,
                charge_amount_kwh REAL NOT NULL
            );
            INSERT INTO charging_stations VALUES (1, '甲站'), (2, '乙站');
            INSERT INTO charging_piles VALUES (1, 1), (2, 1), (3, 2), (4, 2);
            """
        )
        start = datetime(2026, 8, 1, 8)
        order_id = 1
        for day in range(21):
            for station_id, hour, amount in ((1, 8, 12.0 + day), (2, 18, 8.0 + day / 2)):
                stamp = (start + timedelta(days=day)).replace(hour=hour)
                connection.execute(
                    "INSERT INTO orders VALUES (?, ?, 'settled', ?, ?)",
                    (order_id, station_id, stamp.isoformat(sep=" "), amount),
                )
                order_id += 1
        connection.commit()
        connection.close()

    def tearDown(self) -> None:
        self.temp.cleanup()

    def test_pipeline_writes_bounded_complete_predictions(self) -> None:
        output = self.root / "analysis-output"
        result = run_pipeline(self.database, output)
        self.assertEqual(result["predictions"]["daily_predictions"], 7)
        self.assertEqual(result["predictions"]["hourly_predictions"], 24)
        self.assertEqual(result["predictions"]["station_predictions"], 2)
        self.assertTrue((output / "models" / "demand_forecast.pkl").exists())
        self.assertTrue((output / "models" / "peak_predict.pkl").exists())

        with (output / "predictions" / "demand_next7.csv").open(
                encoding="utf-8", newline="") as handle:
            daily = list(csv.DictReader(handle))
        self.assertEqual(len(daily), 7)
        self.assertTrue(all(float(row["predicted_kwh"]) >= 0 for row in daily))
        self.assertTrue(all(float(row["lower_bound"]) <= float(row["upper_bound"])
                            for row in daily))

        with (output / "predictions" / "peak_hours.csv").open(
                encoding="utf-8", newline="") as handle:
            hourly = list(csv.DictReader(handle))
        self.assertEqual(len(hourly), 24)
        self.assertTrue({row["peak_level"] for row in hourly}
                        <= {"peak", "flat", "valley"})

        with (output / "predictions" / "idle_predict.csv").open(
                encoding="utf-8", newline="") as handle:
            stations = list(csv.DictReader(handle))
        self.assertEqual(len(stations), 2)
        for station in stations:
            pile_count = int(station["pile_count"])
            for offset in range(24):
                self.assertGreaterEqual(int(station[f"slot_{offset}"]), 0)
                self.assertLessEqual(int(station[f"slot_{offset}"]), pile_count)


if __name__ == "__main__":
    unittest.main()
