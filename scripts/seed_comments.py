#!/usr/bin/env python3
"""Seed random station comments and likes into the runtime SQLite database.

Usage (in the VM, with the server stopped):

    python3 scripts/seed_comments.py [path/to/ev_charging.sqlite3]

Defaults to data/ev_charging.sqlite3 relative to the repository root.
The script creates the comment tables when missing and is safe to re-run:
duplicate (station_id, user_id) pairs are ignored via INSERT OR IGNORE.
"""

import random
import sqlite3
import sys
from datetime import datetime, timedelta
from pathlib import Path

DEFAULT_DB = Path(__file__).resolve().parent.parent / "data" / "ev_charging.sqlite3"

DDL = [
    """CREATE TABLE IF NOT EXISTS station_comments (
        comment_id INTEGER PRIMARY KEY AUTOINCREMENT,
        station_id INTEGER NOT NULL REFERENCES charging_stations(station_id),
        user_id INTEGER NOT NULL REFERENCES users(user_id),
        content TEXT NOT NULL,
        rating INTEGER NOT NULL,
        created_at TEXT NOT NULL,
        updated_at TEXT NOT NULL,
        UNIQUE (station_id, user_id))""",
    """CREATE TABLE IF NOT EXISTS comment_likes (
        like_id INTEGER PRIMARY KEY AUTOINCREMENT,
        comment_id INTEGER NOT NULL REFERENCES station_comments(comment_id),
        user_id INTEGER NOT NULL REFERENCES users(user_id),
        created_at TEXT NOT NULL,
        UNIQUE (comment_id, user_id))""",
    "CREATE INDEX IF NOT EXISTS idx_station_comments_station ON station_comments(station_id)",
    "CREATE INDEX IF NOT EXISTS idx_comment_likes_comment ON comment_likes(comment_id)",
]

# (content, rating in stars). Positive reviews dominate, mirroring reality.
POOL = [
    ("充电速度飞快，半小时从20%充到80%", 5),
    ("位置好找，停车场入口很显眼", 5),
    ("桩很新，充电过程稳定没断过", 5),
    ("性价比很高，比周边几个站都便宜", 5),
    ("环境不错，等充电的时候能坐便利店歇会儿", 5),
    ("充电正常，就是车位有点窄，大车不好停", 4),
    ("整体不错，就是中午来要排队", 4),
    ("充电功率稳定， app 上看进度也方便", 4),
    ("有两个桩显示故障，希望尽快修好", 3),
    ("充电功率一般，比标称低不少", 3),
    ("位置有点偏，绕了两圈才找到", 2),
    ("桩有点旧，充电枪卡扣接触不良", 2),
    ("晚上照明太差，找桩全靠手机手电", 2),
    ("充电频繁跳枪，体验很差，不推荐", 1),
    ("客服电话永远打不通，出问题没人管", 1),
]


def main() -> int:
    db_path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_DB
    if not db_path.exists():
        print(f"database not found: {db_path}")
        return 1

    connection = sqlite3.connect(db_path)
    connection.execute("PRAGMA foreign_keys = ON")
    for statement in DDL:
        connection.execute(statement)

    users = [row[0] for row in connection.execute(
        "SELECT user_id FROM users ORDER BY user_id")]
    stations = [row[0] for row in connection.execute(
        "SELECT station_id FROM charging_stations ORDER BY station_id")]
    if not users:
        print("users table is empty - register a user first or seed test users")
        return 1
    if not stations:
        print("charging_stations table is empty")
        return 1

    random.seed()
    now = datetime.now()
    comment_count = 0
    like_count = 0
    for station_id in stations:
        chosen = random.sample(users, min(random.randint(2, 6), len(users)))
        for user_id in chosen:
            content, rating = random.choice(POOL)
            created = (now - timedelta(days=random.randint(0, 90),
                                       seconds=random.randint(0, 86400))
                       ).strftime("%Y-%m-%d %H:%M:%S")
            cursor = connection.execute(
                "INSERT OR IGNORE INTO station_comments "
                "(station_id, user_id, content, rating, created_at, updated_at) "
                "VALUES (?, ?, ?, ?, ?, ?)",
                (station_id, user_id, content, rating, created, created))
            comment_count += cursor.rowcount
            if cursor.rowcount:
                likers = random.sample(
                    users, min(random.randint(0, 12), len(users)))
                for liker in likers:
                    if liker == user_id:
                        continue
                    like_cursor = connection.execute(
                        "INSERT OR IGNORE INTO comment_likes "
                        "(comment_id, user_id, created_at) VALUES (?, ?, ?)",
                        (cursor.lastrowid, liker, created))
                    like_count += like_cursor.rowcount
    connection.commit()
    connection.close()
    print(f"seeded {comment_count} comments and {like_count} likes into {db_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
