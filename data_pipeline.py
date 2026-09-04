#!/usr/bin/env python3
"""EVCS 可复现数据采集、预处理和统计分析流水线（UML-50~53）。"""

from __future__ import annotations

import argparse
import json
import sqlite3
from pathlib import Path

import pandas as pd


TABLE_EXPORTS = {
    "users": "users.csv",
    "stations": "stations.csv",
    "chargers": "piles.csv",
    "orders": "orders.csv",
}


def ensure_layout(data_root: Path) -> dict[str, Path]:
    paths = {name: data_root / name for name in ("raw", "processed", "analysis", "models", "predictions")}
    for path in paths.values():
        path.mkdir(parents=True, exist_ok=True)
    return paths


def collect(database: Path, data_root: Path) -> dict[str, int]:
    """以 SQLite 只读连接导出四张业务表，固定按主键排序。"""
    paths = ensure_layout(data_root)
    counts: dict[str, int] = {}
    connection = sqlite3.connect(f"file:{database.resolve()}?mode=ro", uri=True)
    try:
        for table, filename in TABLE_EXPORTS.items():
            columns = pd.read_sql_query(f"PRAGMA table_info({table})", connection)
            if columns.empty:
                raise RuntimeError(f"业务数据库缺少表：{table}")
            primary_key = columns.loc[columns["pk"] == 1, "name"]
            order_by = primary_key.iloc[0] if not primary_key.empty else columns.iloc[0]["name"]
            frame = pd.read_sql_query(f"SELECT * FROM {table} ORDER BY {order_by}", connection)
            frame.to_csv(paths["raw"] / filename, index=False, encoding="utf-8-sig")
            counts[table] = len(frame)
    finally:
        connection.close()
    return counts


def _read(raw: Path, name: str) -> pd.DataFrame:
    path = raw / name
    if not path.is_file():
        raise RuntimeError(f"缺少原始数据：{path}")
    return pd.read_csv(path, encoding="utf-8-sig")


def preprocess(data_root: Path) -> dict[str, int]:
    """清洗异常值、解析时间并生成日/时/站点及模型通用特征。"""
    paths = ensure_layout(data_root)
    orders = _read(paths["raw"], "orders.csv")
    users = _read(paths["raw"], "users.csv")
    stations = _read(paths["raw"], "stations.csv")
    piles = _read(paths["raw"], "piles.csv")

    original_rows = len(orders)
    quality: dict[str, object] = {
        "sourceRows": {"users": len(users), "stations": len(stations),
                       "piles": len(piles), "orders": original_rows},
        "missingRate": {
            "users": users.isna().mean().round(4).to_dict(),
            "orders": orders.isna().mean().round(4).to_dict(),
        },
    }
    display_column = "display_name" if "display_name" in users else "nickname"
    if display_column not in users:
        display_column = "display_name"
        users[display_column] = "未知用户"
    users[display_column] = users[display_column].fillna("未知用户").astype(str).str.strip()
    users.loc[users[display_column] == "", display_column] = "未知用户"
    users = users.drop_duplicates(subset=["id"] if "id" in users else None, keep="last")

    duplicate_key = "order_no" if "order_no" in orders else "id"
    duplicate_count = int(orders.duplicated(subset=[duplicate_key]).sum())
    orders = orders.drop_duplicates(subset=[duplicate_key], keep="last").copy()

    for column in ("energy_wh", "amount_cents", "price_cents_per_kwh"):
        if column not in orders:
            orders[column] = 0
        orders[column] = pd.to_numeric(orders[column], errors="coerce")
    settled_mask = orders.get("status", pd.Series(index=orders.index, dtype="object")).isin(
        ["settled", "paid"])
    missing_settled_energy = int((settled_mask & orders["energy_wh"].isna()).sum())
    orders.loc[settled_mask, ["energy_wh", "amount_cents"]] = (
        orders.loc[settled_mask, ["energy_wh", "amount_cents"]].fillna(0))
    orders = orders.dropna(subset=["energy_wh", "amount_cents", "price_cents_per_kwh"])
    orders = orders[(orders["energy_wh"] >= 0) & (orders["amount_cents"] >= 0)].copy()
    status = orders.get("status", pd.Series(index=orders.index, dtype="object")).fillna("")
    orders = orders[status.isin(["settled", "paid"])].copy()

    time_candidates = [name for name in ("settled_at", "ended_at", "created_at") if name in orders]
    if not time_candidates:
        raise RuntimeError("订单数据缺少可用时间字段")
    orders["event_time"] = pd.Series(
        pd.NaT, index=orders.index, dtype="datetime64[ns, UTC]")
    for column in time_candidates:
        parsed = pd.to_datetime(orders[column], errors="coerce", utc=True)
        orders["event_time"] = orders["event_time"].fillna(parsed)
    orders = orders.dropna(subset=["event_time"]).copy()
    orders["date"] = orders["event_time"].dt.strftime("%Y-%m-%d")
    orders["hour"] = orders["event_time"].dt.hour
    orders["day_of_week"] = orders["event_time"].dt.dayofweek
    orders["is_weekend"] = (orders["day_of_week"] >= 5).astype(int)
    orders["month"] = orders["event_time"].dt.month
    orders["energy_kwh"] = orders["energy_wh"] / 1000.0
    orders["revenue_yuan"] = orders["amount_cents"] / 100.0
    orders["time_slot"] = pd.cut(
        orders["hour"], bins=[-1, 5, 9, 16, 20, 23],
        labels=["night", "morning_peak", "daytime", "evening_peak", "late_evening"]
    ).astype("string")

    if "started_at" in orders and "ended_at" in orders:
        started = pd.to_datetime(orders["started_at"], errors="coerce", utc=True)
        ended = pd.to_datetime(orders["ended_at"], errors="coerce", utc=True)
        orders["duration_seconds"] = (ended - started).dt.total_seconds().clip(lower=0).fillna(0)
    else:
        orders["duration_seconds"] = 0.0

    station_names = stations[["id", "name"]].rename(columns={"id": "station_id", "name": "station_name"})
    pile_features = piles[[column for column in ("id", "station_id", "rated_power_kw", "connector_type") if column in piles]].copy()
    pile_features = pile_features.rename(columns={"id": "charger_id"})
    orders = orders.merge(station_names, on="station_id", how="left")
    if "charger_id" in pile_features:
        orders = orders.merge(pile_features, on=["charger_id", "station_id"], how="left")
    orders["charge_speed"] = pd.Series("unknown", index=orders.index, dtype="string")
    if "rated_power_kw" in orders:
        orders.loc[orders["rated_power_kw"] >= 60, "charge_speed"] = "fast"
        orders.loc[orders["rated_power_kw"] < 60, "charge_speed"] = "slow"

    daily = orders.groupby("date", as_index=False).agg(
        order_count=("id", "count"), energy_kwh=("energy_kwh", "sum"), revenue_yuan=("revenue_yuan", "sum"))
    hourly = orders.groupby(["date", "hour"], as_index=False).agg(
        order_count=("id", "count"), energy_kwh=("energy_kwh", "sum"), revenue_yuan=("revenue_yuan", "sum"))
    station = orders.groupby(["station_id", "station_name"], dropna=False, as_index=False).agg(
        order_count=("id", "count"), total_energy_kwh=("energy_kwh", "sum"),
        total_revenue_yuan=("revenue_yuan", "sum"), average_energy_kwh=("energy_kwh", "mean"))
    station_daily = orders.groupby(["date", "station_id", "station_name"], dropna=False,
                                   as_index=False).agg(
        order_count=("id", "count"), energy_kwh=("energy_kwh", "sum"),
        revenue_yuan=("revenue_yuan", "sum"),
        duration_seconds=("duration_seconds", "sum"))

    users.to_csv(paths["processed"] / "clean_users.csv", index=False, encoding="utf-8-sig")
    orders.to_csv(paths["processed"] / "clean_orders.csv", index=False, encoding="utf-8-sig")
    daily.to_csv(paths["processed"] / "daily_kwh.csv", index=False, encoding="utf-8-sig")
    hourly.to_csv(paths["processed"] / "hourly_kwh.csv", index=False, encoding="utf-8-sig")
    station_daily.to_csv(paths["processed"] / "station_daily.csv", index=False, encoding="utf-8-sig")
    station.to_csv(paths["processed"] / "station_features.csv", index=False, encoding="utf-8-sig")
    orders.to_csv(paths["processed"] / "model_features.csv", index=False, encoding="utf-8-sig")
    quality["cleaning"] = {
        "duplicateOrdersRemoved": duplicate_count,
        "settledMissingEnergyFilledWithZero": missing_settled_energy,
        "ordersRemoved": original_rows - len(orders),
        "validSettledOrders": len(orders),
    }
    (paths["processed"] / "quality_report.json").write_text(
        json.dumps(quality, ensure_ascii=False, indent=2), encoding="utf-8")
    return {"valid_orders": len(orders), "daily_rows": len(daily), "hourly_rows": len(hourly)}


def analyze(data_root: Path) -> dict[str, float | int]:
    """生成站点、用户、时段和营收统计文件及可复核 PNG 图。"""
    paths = ensure_layout(data_root)
    features = pd.read_csv(paths["processed"] / "model_features.csv", encoding="utf-8-sig")
    station_stats = features.groupby(["station_id", "station_name"], dropna=False, as_index=False).agg(
        order_count=("id", "count"), total_energy_kwh=("energy_kwh", "sum"),
        total_revenue_yuan=("revenue_yuan", "sum"), average_energy_kwh=("energy_kwh", "mean"),
        active_days=("date", "nunique"), total_duration_seconds=("duration_seconds", "sum"))
    station_stats["average_daily_energy_kwh"] = (
        station_stats["total_energy_kwh"] / station_stats["active_days"].clip(lower=1))
    station_stats["average_daily_orders"] = (
        station_stats["order_count"] / station_stats["active_days"].clip(lower=1))
    pile_stats = features.groupby("station_id", as_index=False).agg(
        pile_count=("charger_id", "nunique"),
        fast_pile_count=("charger_id", lambda ids: features.loc[ids.index]
                         .query("charge_speed == 'fast'")["charger_id"].nunique()),
        slow_pile_count=("charger_id", lambda ids: features.loc[ids.index]
                         .query("charge_speed == 'slow'")["charger_id"].nunique()))
    station_stats = station_stats.merge(pile_stats, on="station_id", how="left")
    station_stats["utilization_rate"] = (
        station_stats["total_duration_seconds"] /
        (station_stats["active_days"].clip(lower=1) * 86400 * station_stats["pile_count"].clip(lower=1)))
    station_stats["fast_pile_ratio"] = (
        station_stats["fast_pile_count"] / station_stats["pile_count"].clip(lower=1))
    station_stats["slow_pile_ratio"] = (
        station_stats["slow_pile_count"] / station_stats["pile_count"].clip(lower=1))
    peak = (features.groupby(["station_id", "hour"], as_index=False)["order_no"].count()
            .sort_values(["station_id", "order_no", "hour"], ascending=[True, False, True])
            .drop_duplicates("station_id").rename(columns={"hour": "peak_hour"}))
    station_stats = station_stats.merge(peak[["station_id", "peak_hour"]], on="station_id", how="left")
    user_stats = features.groupby("user_id", as_index=False).agg(
        order_count=("id", "count"), total_energy_kwh=("energy_kwh", "sum"),
        total_spend_yuan=("revenue_yuan", "sum"), average_energy_kwh=("energy_kwh", "mean"),
        active_days=("date", "nunique"), first_charge=("event_time", "min"),
        last_charge=("event_time", "max"))
    user_stats["orders_per_active_day"] = (
        user_stats["order_count"] / user_stats["active_days"].clip(lower=1))
    hourly = features.groupby("hour", as_index=False).agg(
        order_count=("id", "count"), energy_kwh=("energy_kwh", "sum"), revenue_yuan=("revenue_yuan", "sum"))
    revenue = features.groupby("date", as_index=False).agg(
        order_count=("id", "count"), energy_kwh=("energy_kwh", "sum"), revenue_yuan=("revenue_yuan", "sum"))
    total_orders = max(1, int(hourly["order_count"].sum()))
    hourly["order_share"] = hourly["order_count"] / total_orders
    revenue["revenue_growth_rate"] = revenue["revenue_yuan"].pct_change().replace(
        [float("inf"), float("-inf")], pd.NA)
    station_stats.to_csv(paths["analysis"] / "station_stats.csv", index=False, encoding="utf-8-sig")
    user_stats.to_csv(paths["analysis"] / "user_stats.csv", index=False, encoding="utf-8-sig")
    hourly.to_csv(paths["analysis"] / "hourly_distribution.csv", index=False, encoding="utf-8-sig")
    revenue.to_csv(paths["analysis"] / "revenue_trend.csv", index=False, encoding="utf-8-sig")

    summary: dict[str, float | int] = {
        "orderCount": int(len(features)),
        "energyKwh": round(float(features["energy_kwh"].sum()), 3),
        "revenueYuan": round(float(features["revenue_yuan"].sum()), 2),
        "stationCount": int(features["station_id"].nunique()),
    }
    (paths["analysis"] / "summary.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")

    chart_specs = (
        (revenue["date"], revenue["revenue_yuan"], "Daily revenue", "revenue_trend.png"),
        (hourly["hour"], hourly["order_count"], "Orders by hour", "hourly_distribution.png"),
        (station_stats["station_name"], station_stats["total_energy_kwh"],
         "Energy by station", "station_energy.png"),
        (user_stats["user_id"].astype(str), user_stats["total_spend_yuan"],
         "User value", "user_value.png"),
    )
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        from matplotlib import font_manager

        # BitDev 的 Noto CJK 是 TTC 字体；显式注册可避免 matplotlib 旧版缓存找不到中文字体。
        cjk_font = Path("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc")
        if cjk_font.exists():
            font_manager.fontManager.addfont(str(cjk_font))
            plt.rcParams["font.family"] = font_manager.FontProperties(fname=str(cjk_font)).get_name()
        else:
            plt.rcParams["font.sans-serif"] = ["WenQuanYi Zen Hei", "DejaVu Sans"]
        plt.rcParams["axes.unicode_minus"] = False
        for x, y, title, filename in chart_specs:
            figure, axis = plt.subplots(figsize=(9, 4.5))
            axis.plot(x, y, marker="o")
            axis.set_title(title)
            axis.grid(alpha=0.25)
            figure.autofmt_xdate()
            figure.tight_layout()
            figure.savefig(paths["analysis"] / filename, dpi=140)
            plt.close(figure)
    except ImportError:
        # 最小虚拟机未安装 matplotlib 时仍生成可验收图；正式分析环境优先使用上面的 matplotlib。
        from PIL import Image, ImageDraw
        for x, y, title, filename in chart_specs:
            values = [float(value) for value in y]
            image = Image.new("RGB", (1260, 630), "white")
            draw = ImageDraw.Draw(image)
            draw.text((50, 24), title, fill="#14213d")
            draw.line((70, 560, 1200, 560), fill="#65758b", width=2)
            draw.line((70, 80, 70, 560), fill="#65758b", width=2)
            maximum = max(values or [1.0]) or 1.0
            points = []
            for index, value in enumerate(values):
                px = 70 + (1130 * index / max(1, len(values) - 1))
                py = 560 - (450 * value / maximum)
                points.append((px, py))
            if len(points) > 1:
                draw.line(points, fill="#1479b8", width=4)
            for point in points:
                draw.ellipse((point[0] - 5, point[1] - 5, point[0] + 5, point[1] + 5), fill="#0b9ea8")
            image.save(paths["analysis"] / filename)
    return summary


def parse_args() -> argparse.Namespace:
    project = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description="EVCS 数据采集、预处理与统计分析")
    parser.add_argument("command", choices=("collect", "preprocess", "analyze", "all"))
    parser.add_argument("--database", type=Path, default=project.parent / "evcs-runtime" / "evcharging.db")
    parser.add_argument("--data-root", type=Path, default=project.parent / "evcs-runtime" / "data")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    result: dict[str, object] = {}
    if args.command in {"collect", "all"}:
        result["collect"] = collect(args.database, args.data_root)
    if args.command in {"preprocess", "all"}:
        result["preprocess"] = preprocess(args.data_root)
    if args.command in {"analyze", "all"}:
        result["analyze"] = analyze(args.data_root)
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
