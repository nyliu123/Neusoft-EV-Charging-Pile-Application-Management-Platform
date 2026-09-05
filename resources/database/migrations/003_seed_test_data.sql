-- Shared demo/test data for development (used by both admin and user clients).
-- Idempotent: fixed primary keys + INSERT OR IGNORE, safe to run on every start.
-- All timestamps use localtime so "近7日" style queries line up with the trend chart.
-- To regenerate fresh data, delete the sqlite file and restart the server.

INSERT OR IGNORE INTO charging_stations
    (station_id, station_name, address, longitude, latitude, price_per_kwh) VALUES
    (1, '东软园区站',     '大连市甘井子区软件园路8号',     121.509605, 38.863650, 1.20),
    (2, '高铁北站广场站', '大连市甘井子区大连北站北广场', 121.570020, 38.951800, 1.50),
    (3, '软件园东区站',   '大连市高新园区数码路东段1号',  121.534800, 38.875200, 1.00);

INSERT OR IGNORE INTO charging_piles
    (pile_id, station_id, pile_number, pile_type, power_kw, status,
     total_charge_count, total_charge_duration) VALUES
    (1,  1, 'A-01', 'fast',  60.0,  'idle',     128, 512.5),
    (2,  1, 'A-02', 'fast',  60.0,  'in_use',    96, 401.2),
    (3,  1, 'B-01', 'slow',   7.0,  'fault',     45, 210.8),
    (4,  1, 'B-02', 'slow',   7.0,  'idle',      38, 176.4),
    (5,  2, 'C-01', 'fast', 120.0,  'idle',     210, 890.1),
    (6,  2, 'C-02', 'fast', 120.0,  'reserved', 188, 795.0),
    (7,  2, 'C-03', 'fast', 120.0,  'in_use',   155, 640.3),
    (8,  2, 'C-04', 'slow',   7.0,  'fault',     22,  98.6),
    (9,  3, 'D-01', 'slow',   7.0,  'idle',      66, 300.9),
    (10, 3, 'D-02', 'slow',   7.0,  'idle',      41, 190.2);

-- 13800138000 / 15812349876 are reserved fake numbers for demos.
INSERT OR IGNORE INTO users
    (user_id, phone, nickname, avatar_path, balance, register_time, status) VALUES
    (1, '13800138000', '用户0001',  NULL, 128.50,
     datetime('now', 'localtime', '-30 days'), 'normal'),
    (2, '13912345678', '阿宇',      NULL, 256.80,
     datetime('now', 'localtime', '-20 days'), 'normal'),
    (3, '13666668888', '大货车司机', NULL,  89.50,
     datetime('now', 'localtime', '-15 days'), 'normal'),
    (4, '15812349876', '黑猫警长',  NULL,  40.00,
     datetime('now', 'localtime', '-10 days'), 'frozen'),
    (5, '15900001111', '小鹿',      NULL,  65.20,
     datetime('now', 'localtime', '-5 days'),  'normal');

-- In-flight orders: one charging, one reserved, one pending settlement.
INSERT OR IGNORE INTO orders
    (order_id, user_id, pile_id, station_id, status, reserve_time, start_time, end_time,
     charge_amount_kwh, price_per_kwh, total_fee) VALUES
    (11, 1, 2, 1, 'charging',
     datetime('now', 'localtime', '-50 minutes'),
     datetime('now', 'localtime', '-48 minutes'), NULL, 0, 1.20, 0),
    (12, 3, 6, 2, 'reserved',
     datetime('now', 'localtime', '-10 minutes'), NULL, NULL, 0, 1.50, 0),
    (14, 5, 10, 3, 'pending_settlement',
     datetime('now', 'localtime', '-3 hours'),
     datetime('now', 'localtime', '-3 hours', '+2 minutes'),
     datetime('now', 'localtime', '-2 hours'), 28.00, 1.00, 28.00),
    (15, 2, 7, 2, 'charging',
     datetime('now', 'localtime', '-20 minutes'),
     datetime('now', 'localtime', '-18 minutes'), NULL, 0, 1.50, 0);

-- History over the last 7 days: 10 settled orders + 1 cancelled.
-- Prices match each station's price_per_kwh.
INSERT OR IGNORE INTO orders
    (order_id, user_id, pile_id, station_id, status, reserve_time, start_time, end_time,
     charge_amount_kwh, price_per_kwh, total_fee) VALUES
    (1, 1, 1, 1, 'settled',
     datetime('now', 'localtime', '-6 days', 'start of day', '+9 hours'),
     datetime('now', 'localtime', '-6 days', 'start of day', '+9 hours', '+2 minutes'),
     datetime('now', 'localtime', '-6 days', 'start of day', '+10 hours'),
     42.50, 1.20, 51.00),
    (2, 2, 5, 2, 'settled',
     datetime('now', 'localtime', '-6 days', 'start of day', '+14 hours'),
     datetime('now', 'localtime', '-6 days', 'start of day', '+14 hours', '+3 minutes'),
     datetime('now', 'localtime', '-6 days', 'start of day', '+15 hours', '+30 minutes'),
     30.00, 1.50, 45.00),
    (3, 1, 4, 1, 'settled',
     datetime('now', 'localtime', '-5 days', 'start of day', '+9 hours', '+10 minutes'),
     datetime('now', 'localtime', '-5 days', 'start of day', '+9 hours', '+12 minutes'),
     datetime('now', 'localtime', '-5 days', 'start of day', '+12 hours', '+40 minutes'),
     24.80, 1.20, 29.76),
    (4, 3, 7, 2, 'settled',
     datetime('now', 'localtime', '-5 days', 'start of day', '+16 hours'),
     datetime('now', 'localtime', '-5 days', 'start of day', '+16 hours', '+3 minutes'),
     datetime('now', 'localtime', '-5 days', 'start of day', '+17 hours'),
     52.00, 1.50, 78.00),
    (5, 2, 9, 3, 'settled',
     datetime('now', 'localtime', '-4 days', 'start of day', '+10 hours', '+5 minutes'),
     datetime('now', 'localtime', '-4 days', 'start of day', '+10 hours', '+7 minutes'),
     datetime('now', 'localtime', '-4 days', 'start of day', '+12 hours'),
     18.60, 1.00, 18.60),
    (6, 1, 1, 1, 'settled',
     datetime('now', 'localtime', '-3 days', 'start of day', '+8 hours', '+30 minutes'),
     datetime('now', 'localtime', '-3 days', 'start of day', '+8 hours', '+32 minutes'),
     datetime('now', 'localtime', '-3 days', 'start of day', '+11 hours', '+20 minutes'),
     38.20, 1.20, 45.84),
    (7, 3, 5, 2, 'settled',
     datetime('now', 'localtime', '-3 days', 'start of day', '+15 hours', '+20 minutes'),
     datetime('now', 'localtime', '-3 days', 'start of day', '+15 hours', '+23 minutes'),
     datetime('now', 'localtime', '-3 days', 'start of day', '+16 hours', '+10 minutes'),
     41.00, 1.50, 61.50),
    (8, 1, 9, 3, 'cancelled',
     datetime('now', 'localtime', '-3 days', 'start of day', '+11 hours'),
     NULL, NULL, 0, 1.00, 0),
    (9, 2, 1, 1, 'settled',
     datetime('now', 'localtime', '-2 days', 'start of day', '+13 hours', '+15 minutes'),
     datetime('now', 'localtime', '-2 days', 'start of day', '+13 hours', '+17 minutes'),
     datetime('now', 'localtime', '-2 days', 'start of day', '+14 hours', '+40 minutes'),
     33.30, 1.20, 39.96),
    (10, 3, 7, 2, 'settled',
     datetime('now', 'localtime', '-1 days', 'start of day', '+9 hours', '+40 minutes'),
     datetime('now', 'localtime', '-1 days', 'start of day', '+9 hours', '+43 minutes'),
     datetime('now', 'localtime', '-1 days', 'start of day', '+10 hours', '+50 minutes'),
     47.50, 1.50, 71.25),
    (13, 1, 4, 1, 'settled',
     datetime('now', 'localtime', 'start of day', '+8 hours', '+20 minutes'),
     datetime('now', 'localtime', 'start of day', '+8 hours', '+22 minutes'),
     datetime('now', 'localtime', 'start of day', '+11 hours', '+35 minutes'),
     21.40, 1.20, 25.68);
