import sqlite3
import tempfile
import unittest
from pathlib import Path

from data_pipeline import analyze, collect, preprocess


class DataPipelineTest(unittest.TestCase):
    def test_full_pipeline_is_repeatable(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            database = root / "evcs.db"
            connection = sqlite3.connect(database)
            connection.executescript(
                """
                CREATE TABLE users(id INTEGER PRIMARY KEY, display_name TEXT);
                CREATE TABLE stations(id INTEGER PRIMARY KEY, name TEXT);
                CREATE TABLE chargers(id INTEGER PRIMARY KEY, station_id INTEGER,
                    rated_power_kw REAL, connector_type TEXT);
                CREATE TABLE orders(id INTEGER PRIMARY KEY, order_no TEXT, user_id INTEGER,
                    station_id INTEGER, charger_id INTEGER, energy_wh INTEGER,
                    price_cents_per_kwh INTEGER, amount_cents INTEGER, status TEXT,
                    settled_at TEXT, started_at TEXT, ended_at TEXT, created_at TEXT);
                INSERT INTO users VALUES(1, '演示用户');
                INSERT INTO stations VALUES(1, '测试站');
                INSERT INTO chargers VALUES(1, 1, 60.0, 'GB/T');
                INSERT INTO orders VALUES
                    (1, 'A', 1, 1, 1, 12000, 100, 1200, 'settled',
                     '2026-09-01T10:00:00Z', '2026-09-01T09:30:00Z',
                     '2026-09-01T10:00:00Z', '2026-09-01T09:30:00Z'),
                    (2, 'B', 1, 1, 1, -1, 100, 0, 'settled',
                     '2026-09-01T11:00:00Z', '2026-09-01T10:30:00Z',
                     '2026-09-01T11:00:00Z', '2026-09-01T10:30:00Z');
                """
            )
            connection.commit()
            connection.close()

            data_root = root / "data"
            self.assertEqual(collect(database, data_root)["orders"], 2)
            self.assertEqual(preprocess(data_root)["valid_orders"], 1)
            summary = analyze(data_root)
            self.assertEqual(summary["orderCount"], 1)
            self.assertTrue((data_root / "analysis" / "revenue_trend.png").is_file())
            self.assertTrue((data_root / "processed" / "station_daily.csv").is_file())
            self.assertTrue((data_root / "processed" / "quality_report.json").is_file())
            self.assertTrue((data_root / "analysis" / "station_energy.png").is_file())


if __name__ == "__main__":
    unittest.main()
