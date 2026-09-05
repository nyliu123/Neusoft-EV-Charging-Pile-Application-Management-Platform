-- Core schema from section 4.1 of the overview design.
-- The admins.password column stores a salted password verifier, never plaintext.

CREATE TABLE IF NOT EXISTS users (
    user_id INTEGER PRIMARY KEY AUTOINCREMENT,
    phone VARCHAR(11) NOT NULL UNIQUE,
    nickname VARCHAR(50),
    avatar_path VARCHAR(255),
    balance DECIMAL(10, 2) NOT NULL DEFAULT 0.00 CHECK (balance >= 0),
    register_time DATETIME NOT NULL DEFAULT (datetime('now')),
    status VARCHAR(10) NOT NULL DEFAULT 'normal'
        CHECK (status IN ('normal', 'frozen'))
);

CREATE TABLE IF NOT EXISTS admins (
    admin_id INTEGER PRIMARY KEY AUTOINCREMENT,
    username VARCHAR(50) NOT NULL UNIQUE,
    password VARCHAR(255) NOT NULL,
    create_time DATETIME NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS charging_stations (
    station_id INTEGER PRIMARY KEY AUTOINCREMENT,
    station_name VARCHAR(100) NOT NULL,
    address VARCHAR(255),
    longitude DECIMAL(10, 6) CHECK (longitude BETWEEN -180 AND 180),
    latitude DECIMAL(10, 6) CHECK (latitude BETWEEN -90 AND 90),
    price_per_kwh DECIMAL(10, 2) NOT NULL CHECK (price_per_kwh > 0)
);

CREATE TABLE IF NOT EXISTS charging_piles (
    pile_id INTEGER PRIMARY KEY AUTOINCREMENT,
    station_id INTEGER NOT NULL REFERENCES charging_stations(station_id),
    pile_number VARCHAR(20) NOT NULL,
    pile_type VARCHAR(10) NOT NULL CHECK (pile_type IN ('fast', 'slow')),
    power_kw DECIMAL(10, 2) NOT NULL CHECK (power_kw > 0),
    status VARCHAR(10) NOT NULL DEFAULT 'idle'
        CHECK (status IN ('idle', 'reserved', 'in_use', 'fault')),
    total_charge_count INTEGER NOT NULL DEFAULT 0 CHECK (total_charge_count >= 0),
    total_charge_duration DECIMAL NOT NULL DEFAULT 0 CHECK (total_charge_duration >= 0),
    UNIQUE (station_id, pile_number)
);

CREATE TABLE IF NOT EXISTS orders (
    order_id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL REFERENCES users(user_id),
    pile_id INTEGER NOT NULL REFERENCES charging_piles(pile_id),
    station_id INTEGER NOT NULL REFERENCES charging_stations(station_id),
    status VARCHAR(20) NOT NULL
        CHECK (status IN ('reserved', 'charging', 'pending_settlement', 'settled', 'cancelled')),
    reserve_time DATETIME NOT NULL DEFAULT (datetime('now')),
    start_time DATETIME,
    end_time DATETIME,
    charge_amount_kwh DECIMAL(10, 2) NOT NULL DEFAULT 0 CHECK (charge_amount_kwh >= 0),
    price_per_kwh DECIMAL(10, 2) NOT NULL CHECK (price_per_kwh > 0),
    total_fee DECIMAL(10, 2) NOT NULL DEFAULT 0 CHECK (total_fee >= 0)
);

CREATE UNIQUE INDEX IF NOT EXISTS ux_orders_active_user
ON orders(user_id)
WHERE status IN ('reserved', 'charging', 'pending_settlement');

CREATE UNIQUE INDEX IF NOT EXISTS ux_orders_active_pile
ON orders(pile_id)
WHERE status IN ('reserved', 'charging');

CREATE INDEX IF NOT EXISTS ix_orders_reserve_time ON orders(reserve_time DESC);
CREATE INDEX IF NOT EXISTS ix_piles_station_status ON charging_piles(station_id, status);

