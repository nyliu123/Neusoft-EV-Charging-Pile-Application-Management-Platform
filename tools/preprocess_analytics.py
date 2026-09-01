#!/usr/bin/env python3
"""Build a deterministic, analysis-ready charging-order dataset from EVCS SQLite data."""

from __future__ import annotations

import argparse
import csv
import json
import sqlite3
from datetime import datetime
from pathlib import Path


OUTPUT_FIELDS = [
    "order_id", "order_no", "user_id", "station_id", "station_name", "charger_id",
    "charger_code", "session_id", "started_at", "ended_at", "duration_seconds",
    "order_created_at", "order_date", "order_hour", "energy_kwh", "amount_yuan",
    "price_yuan_per_kwh", "order_status",
]


def parse_time(value: str | None) -> datetime | None:
    if not value:
        return None
    try:
        return datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError:
        return None


def preprocess(database_path: Path, output_dir: Path) -> dict:
    output_dir.mkdir(parents=True, exist_ok=True)
    connection = sqlite3.connect(f"file:{database_path}?mode=ro", uri=True)
    connection.row_factory = sqlite3.Row
    rows = connection.execute(
        """
        SELECT o.id AS order_id, o.order_no, o.user_id, o.station_id, s.name AS station_name,
               o.charger_id, c.code AS charger_code, cs.id AS session_id,
               cs.started_at, cs.ended_at, o.created_at AS order_created_at,
               o.energy_wh, o.amount_cents, cs.price_cents_per_kwh, o.status AS order_status
        FROM orders o
        JOIN charging_sessions cs ON cs.id = o.charging_session_id
        JOIN stations s ON s.id = o.station_id
        JOIN chargers c ON c.id = o.charger_id
        ORDER BY o.id
        """
    ).fetchall()
    connection.close()

    rejected_by_reason: dict[str, int] = {}
    seen_order_numbers: set[str] = set()
    clean_rows: list[dict] = []

    def reject(reason: str) -> None:
        rejected_by_reason[reason] = rejected_by_reason.get(reason, 0) + 1

    for row in rows:
        order_no = (row["order_no"] or "").strip()
        if not order_no:
            reject("missing_order_no")
            continue
        if order_no in seen_order_numbers:
            reject("duplicate_order_no")
            continue
        seen_order_numbers.add(order_no)
        started = parse_time(row["started_at"])
        ended = parse_time(row["ended_at"])
        created = parse_time(row["order_created_at"])
        if not started or not ended or not created:
            reject("invalid_or_missing_timestamp")
            continue
        duration = int((ended - started).total_seconds())
        if duration < 0:
            reject("negative_duration")
            continue
        if row["energy_wh"] < 0 or row["amount_cents"] < 0 or row["price_cents_per_kwh"] < 0:
            reject("negative_measurement")
            continue
        clean_rows.append({
            "order_id": row["order_id"],
            "order_no": order_no,
            "user_id": row["user_id"],
            "station_id": row["station_id"],
            "station_name": row["station_name"],
            "charger_id": row["charger_id"],
            "charger_code": row["charger_code"],
            "session_id": row["session_id"],
            "started_at": started.isoformat(),
            "ended_at": ended.isoformat(),
            "duration_seconds": duration,
            "order_created_at": created.isoformat(),
            "order_date": created.date().isoformat(),
            "order_hour": created.hour,
            "energy_kwh": round(row["energy_wh"] / 1000.0, 3),
            "amount_yuan": round(row["amount_cents"] / 100.0, 2),
            "price_yuan_per_kwh": round(row["price_cents_per_kwh"] / 100.0, 2),
            "order_status": row["order_status"],
        })

    csv_path = output_dir / "charging_analytics.csv"
    with csv_path.open("w", encoding="utf-8-sig", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=OUTPUT_FIELDS)
        writer.writeheader()
        writer.writerows(clean_rows)

    report = {
        "pipelineVersion": "1.0",
        "sourceDatabase": str(database_path.resolve()),
        "inputRows": len(rows),
        "outputRows": len(clean_rows),
        "rejectedRows": len(rows) - len(clean_rows),
        "rejectedByReason": rejected_by_reason,
        "outputFile": str(csv_path.resolve()),
        "fields": OUTPUT_FIELDS,
        "notes": [
            "Only completed orders with valid joined station, charger and session records are exported.",
            "No missing values are statistically imputed and no machine-learning labels are fabricated.",
        ],
    }
    report_path = output_dir / "data_quality_report.json"
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description="Preprocess EVCS charging data for analysis")
    parser.add_argument("--database", required=True, type=Path, help="EVCS SQLite database")
    parser.add_argument("--output-dir", required=True, type=Path, help="Output directory")
    args = parser.parse_args()
    if not args.database.is_file():
        parser.error(f"database does not exist: {args.database}")
    report = preprocess(args.database, args.output_dir)
    print(json.dumps(report, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
