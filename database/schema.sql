PRAGMA foreign_keys = ON;
PRAGMA journal_mode = WAL;

CREATE TABLE IF NOT EXISTS schema_version (
    version INTEGER PRIMARY KEY,
    applied_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    password_salt TEXT NOT NULL,
    role TEXT NOT NULL CHECK (role IN ('user', 'admin')),
    display_name TEXT NOT NULL DEFAULT '',
    phone TEXT NOT NULL DEFAULT '',
    avatar_mime TEXT NOT NULL DEFAULT '',
    avatar_data BLOB,
    balance_cents INTEGER NOT NULL DEFAULT 0 CHECK (balance_cents >= 0),
    status TEXT NOT NULL DEFAULT 'active' CHECK (status IN ('active', 'disabled')),
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS sessions (
    token TEXT PRIMARY KEY,
    user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    expires_at TEXT NOT NULL,
    created_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS stations (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    region TEXT NOT NULL DEFAULT '',
    address TEXT NOT NULL,
    longitude REAL NOT NULL,
    latitude REAL NOT NULL,
    business_hours TEXT NOT NULL DEFAULT '00:00-24:00',
    status TEXT NOT NULL DEFAULT 'active' CHECK (status IN ('active', 'disabled')),
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS tariffs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    price_cents_per_kwh INTEGER NOT NULL CHECK (price_cents_per_kwh >= 0),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS chargers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    station_id INTEGER NOT NULL REFERENCES stations(id) ON DELETE CASCADE,
    code TEXT NOT NULL UNIQUE,
    connector_type TEXT NOT NULL DEFAULT 'GB/T',
    rated_power_kw REAL NOT NULL DEFAULT 7.0 CHECK (rated_power_kw > 0),
    status TEXT NOT NULL DEFAULT 'idle' CHECK (status IN ('idle', 'reserved', 'charging', 'fault', 'offline', 'disabled')),
    tariff_id INTEGER NOT NULL REFERENCES tariffs(id),
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS reservations (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL REFERENCES users(id),
    charger_id INTEGER NOT NULL REFERENCES chargers(id),
    status TEXT NOT NULL DEFAULT 'active' CHECK (status IN ('active', 'used', 'cancelled', 'expired')),
    reserved_at TEXT NOT NULL,
    expires_at TEXT NOT NULL,
    completed_at TEXT
);

CREATE TABLE IF NOT EXISTS charging_sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL REFERENCES users(id),
    charger_id INTEGER NOT NULL REFERENCES chargers(id),
    reservation_id INTEGER REFERENCES reservations(id),
    status TEXT NOT NULL DEFAULT 'charging' CHECK (status IN ('charging', 'finished', 'interrupted')),
    started_at TEXT NOT NULL,
    ended_at TEXT,
    energy_wh INTEGER NOT NULL DEFAULT 0 CHECK (energy_wh >= 0),
    price_cents_per_kwh INTEGER NOT NULL CHECK (price_cents_per_kwh >= 0),
    amount_cents INTEGER NOT NULL DEFAULT 0 CHECK (amount_cents >= 0)
);

CREATE TABLE IF NOT EXISTS orders (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    order_no TEXT NOT NULL UNIQUE,
    charging_session_id INTEGER NOT NULL UNIQUE REFERENCES charging_sessions(id),
    user_id INTEGER NOT NULL REFERENCES users(id),
    station_id INTEGER NOT NULL REFERENCES stations(id),
    charger_id INTEGER NOT NULL REFERENCES chargers(id),
    energy_wh INTEGER NOT NULL CHECK (energy_wh >= 0),
    amount_cents INTEGER NOT NULL CHECK (amount_cents >= 0),
    status TEXT NOT NULL DEFAULT 'paid' CHECK (status IN ('pending', 'paid', 'cancelled')),
    created_at TEXT NOT NULL,
    paid_at TEXT
);

CREATE TABLE IF NOT EXISTS fault_reports (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    charger_id INTEGER NOT NULL REFERENCES chargers(id),
    title TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    status TEXT NOT NULL DEFAULT 'open' CHECK (status IN ('open', 'processing', 'resolved')),
    reported_at TEXT NOT NULL,
    resolved_at TEXT
);

CREATE TABLE IF NOT EXISTS recharge_records (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL REFERENCES users(id),
    amount_cents INTEGER NOT NULL CHECK (amount_cents > 0),
    balance_after_cents INTEGER NOT NULL CHECK (balance_after_cents >= 0),
    channel TEXT NOT NULL DEFAULT 'demo',
    created_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS charger_operation_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    charger_id INTEGER NOT NULL REFERENCES chargers(id),
    operator_user_id INTEGER NOT NULL REFERENCES users(id),
    operation TEXT NOT NULL,
    previous_status TEXT NOT NULL,
    result_status TEXT NOT NULL,
    success INTEGER NOT NULL CHECK (success IN (0, 1)),
    message TEXT NOT NULL DEFAULT '',
    created_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS audit_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    actor_user_id INTEGER REFERENCES users(id),
    action TEXT NOT NULL,
    entity_type TEXT NOT NULL DEFAULT '',
    entity_id TEXT NOT NULL DEFAULT '',
    detail_json TEXT NOT NULL DEFAULT '{}',
    created_at TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_chargers_station_status ON chargers(station_id, status);
CREATE INDEX IF NOT EXISTS idx_reservations_user_status ON reservations(user_id, status);
CREATE INDEX IF NOT EXISTS idx_reservations_expiry ON reservations(status, expires_at);
CREATE INDEX IF NOT EXISTS idx_sessions_user_status ON charging_sessions(user_id, status);
CREATE INDEX IF NOT EXISTS idx_orders_user_created ON orders(user_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_faults_charger_status ON fault_reports(charger_id, status);
CREATE UNIQUE INDEX IF NOT EXISTS idx_users_phone_unique ON users(phone) WHERE phone <> '';
CREATE INDEX IF NOT EXISTS idx_recharges_user_created ON recharge_records(user_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_charger_operations_created ON charger_operation_logs(charger_id, created_at DESC);
