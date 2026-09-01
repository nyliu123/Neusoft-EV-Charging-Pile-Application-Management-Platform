#!/usr/bin/env python3

import csv
import json
import sqlite3
import subprocess
import sys
import tempfile
from pathlib import Path


def main() -> int:
    script = Path(sys.argv[1])
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        database = root / "test.db"
        output = root / "processed"
        connection = sqlite3.connect(database)
        connection.executescript(
            """
            CREATE TABLE stations(id INTEGER PRIMARY KEY, name TEXT);
            CREATE TABLE chargers(id INTEGER PRIMARY KEY, code TEXT);
            CREATE TABLE charging_sessions(
                id INTEGER PRIMARY KEY, started_at TEXT, ended_at TEXT,
                price_cents_per_kwh INTEGER
            );
            CREATE TABLE orders(
                id INTEGER PRIMARY KEY, order_no TEXT, user_id INTEGER, station_id INTEGER,
                charger_id INTEGER, charging_session_id INTEGER, energy_wh INTEGER,
                amount_cents INTEGER, status TEXT, created_at TEXT
            );
            INSERT INTO stations VALUES(1, '测试站');
            INSERT INTO chargers VALUES(1, 'TEST-001');
            INSERT INTO charging_sessions VALUES(1, '2026-08-01T10:00:00+00:00', '2026-08-01T10:30:00+00:00', 120);
            INSERT INTO charging_sessions VALUES(2, 'bad-time', '2026-08-01T11:30:00+00:00', 120);
            INSERT INTO orders VALUES(1, 'ORDER-1', 1, 1, 1, 1, 12500, 1500, 'paid', '2026-08-01T10:30:00+00:00');
            INSERT INTO orders VALUES(2, 'ORDER-2', 1, 1, 1, 2, 1000, 120, 'paid', '2026-08-01T11:30:00+00:00');
            """
        )
        connection.commit()
        connection.close()
        result = subprocess.run(
            [sys.executable, str(script), "--database", str(database), "--output-dir", str(output)],
            check=True, capture_output=True, text=True,
        )
        assert result.returncode == 0
        report = json.loads((output / "data_quality_report.json").read_text(encoding="utf-8"))
        assert report["inputRows"] == 2
        assert report["outputRows"] == 1
        assert report["rejectedByReason"] == {"invalid_or_missing_timestamp": 1}
        with (output / "charging_analytics.csv").open(encoding="utf-8-sig", newline="") as source:
            rows = list(csv.DictReader(source))
        assert len(rows) == 1
        assert rows[0]["duration_seconds"] == "1800"
        assert rows[0]["energy_kwh"] == "12.5"
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
