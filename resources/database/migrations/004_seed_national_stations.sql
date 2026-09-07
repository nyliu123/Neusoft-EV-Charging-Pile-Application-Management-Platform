-- Compact nationwide teaching data: 26 stations, ten piles per station.
-- Dalian keeps A; three additional provincial-level stations use B-D; the
-- remaining 22 province stations use E-Z.

DROP TRIGGER IF EXISTS trg_station_create_ten_piles;
DROP TRIGGER IF EXISTS trg_station_reject_eleventh_pile;

-- Remove only stations created by the former large demonstration-data migration.
DELETE FROM orders
WHERE station_id IN (
    SELECT station_id FROM charging_stations
    WHERE station_name GLOB '*示范站[0-9][0-9]'
);
DELETE FROM charging_piles
WHERE station_id IN (
    SELECT station_id FROM charging_stations
    WHERE station_name GLOB '*示范站[0-9][0-9]'
);
DELETE FROM charging_stations
WHERE station_name GLOB '*示范站[0-9][0-9]';

-- Convert the other two original Dalian records into provincial-level stations.
-- Keeping their primary keys also keeps the demonstration orders valid.
UPDATE charging_stations
SET station_name = '北京充电中心', address = '北京市朝阳区建国路',
    longitude = 116.407387, latitude = 39.904179, price_per_kwh = 1.55
WHERE station_id = 2;
UPDATE charging_stations
SET station_name = '上海充电中心', address = '上海市浦东新区世纪大道',
    longitude = 121.473701, latitude = 31.230416, price_per_kwh = 1.55
WHERE station_id = 3;

-- Normalize the original piles without changing their primary keys.
UPDATE charging_piles
SET pile_number = 'TMP-' || pile_id
WHERE station_id IN (1, 2, 3);

WITH ranked AS (
    SELECT pile_id, station_id,
           ROW_NUMBER() OVER (PARTITION BY station_id ORDER BY pile_id) AS ordinal
    FROM charging_piles
    WHERE station_id IN (1, 2, 3)
)
UPDATE charging_piles
SET pile_number = (
    SELECT CASE
        WHEN ranked.station_id = 1
            THEN 'A-' || printf('%02d', ranked.ordinal)
        WHEN ranked.station_id = 2
            THEN 'B-' || printf('%02d', ranked.ordinal)
        ELSE 'C-' || printf('%02d', ranked.ordinal)
    END
    FROM ranked WHERE ranked.pile_id = charging_piles.pile_id
)
WHERE station_id IN (1, 2, 3);

-- D is assigned to Chongqing; E-Z remain the 22 province stations.
INSERT OR IGNORE INTO charging_stations
    (station_id, station_name, address, longitude, latitude, price_per_kwh) VALUES
    (26, '重庆充电中心',   '重庆市渝北区金开大道',           106.551556, 29.563009, 1.28),
    (4,  '石家庄充电中心', '河北省石家庄市长安区中山东路', 114.514860, 38.042307, 1.20),
    (5,  '太原充电中心',   '山西省太原市小店区长风街',     112.549248, 37.857014, 1.16),
    (6,  '长春充电中心',   '吉林省长春市南关区人民大街',   125.323544, 43.817071, 1.18),
    (7,  '哈尔滨充电中心', '黑龙江省哈尔滨市南岗区红旗大街',126.534967, 45.803775, 1.22),
    (8,  '南京充电中心',   '江苏省南京市建邺区江东中路',   118.796877, 32.060255, 1.35),
    (9,  '杭州充电中心',   '浙江省杭州市西湖区文三路',     120.155070, 30.274084, 1.38),
    (10, '合肥充电中心',   '安徽省合肥市蜀山区潜山路',     117.227239, 31.820586, 1.20),
    (11, '福州充电中心',   '福建省福州市鼓楼区五四路',     119.296494, 26.074507, 1.28),
    (12, '南昌充电中心',   '江西省南昌市红谷滩区丰和中大道',115.858197, 28.682892, 1.17),
    (13, '济南充电中心',   '山东省济南市历下区经十路',     117.120128, 36.652069, 1.25),
    (14, '郑州充电中心',   '河南省郑州市金水区金水路',     113.625368, 34.746599, 1.18),
    (15, '武汉充电中心',   '湖北省武汉市武昌区中北路',     114.305393, 30.593099, 1.24),
    (16, '长沙充电中心',   '湖南省长沙市岳麓区潇湘大道',   112.938814, 28.228209, 1.22),
    (17, '广州充电中心',   '广东省广州市天河区黄埔大道',   113.264385, 23.129112, 1.42),
    (18, '海口充电中心',   '海南省海口市龙华区滨海大道',   110.198293, 20.044001, 1.26),
    (19, '成都充电中心',   '四川省成都市武侯区天府大道',   104.066541, 30.572269, 1.23),
    (20, '贵阳充电中心',   '贵州省贵阳市观山湖区林城东路', 106.630153, 26.647661, 1.15),
    (21, '昆明充电中心',   '云南省昆明市盘龙区北京路',     102.832891, 24.880095, 1.16),
    (22, '西安充电中心',   '陕西省西安市雁塔区科技路',     108.939770, 34.341574, 1.21),
    (23, '兰州充电中心',   '甘肃省兰州市城关区南滨河东路', 103.834303, 36.061089, 1.14),
    (24, '西宁充电中心',   '青海省西宁市城西区五四西路',   101.778228, 36.617144, 1.12),
    (25, '台北充电中心',   '台湾省台北市信义区市府路',     121.565418, 25.032969, 1.36);

-- Every current station owns one letter and ten numbered piles.
WITH RECURSIVE sequence(ordinal) AS (
    VALUES(1) UNION ALL SELECT ordinal + 1 FROM sequence WHERE ordinal < 10
), station_letters(station_id, prefix) AS (
    VALUES (1,'A'), (2,'B'), (3,'C'), (26,'D'),
           (4,'E'), (5,'F'), (6,'G'), (7,'H'), (8,'I'), (9,'J'),
           (10,'K'), (11,'L'), (12,'M'), (13,'N'), (14,'O'), (15,'P'),
           (16,'Q'), (17,'R'), (18,'S'), (19,'T'), (20,'U'), (21,'V'),
           (22,'W'), (23,'X'), (24,'Y'), (25,'Z')
)
INSERT INTO charging_piles
    (station_id, pile_number, pile_type, power_kw, status,
     total_charge_count, total_charge_duration)
SELECT station_letters.station_id,
       station_letters.prefix || '-' || printf('%02d', sequence.ordinal),
       CASE WHEN sequence.ordinal <= 6 THEN 'fast' ELSE 'slow' END,
       CASE WHEN sequence.ordinal <= 6 THEN 120.0 ELSE 7.0 END,
       CASE WHEN sequence.ordinal <= 5 THEN 'idle'
            WHEN sequence.ordinal <= 7 THEN 'in_use'
            WHEN sequence.ordinal = 8 THEN 'reserved'
            ELSE 'fault' END,
       0, 0
FROM station_letters CROSS JOIN sequence
WHERE NOT EXISTS (
    SELECT 1 FROM charging_piles p
    WHERE p.station_id = station_letters.station_id
      AND p.pile_number = station_letters.prefix || '-' || printf('%02d', sequence.ordinal)
);

-- Deleting the former bulk seed must not leave the autoincrement counter near 300.
-- Keep genuine higher IDs if a real station was added independently.
UPDATE sqlite_sequence
SET seq = (SELECT MAX(station_id) FROM charging_stations)
WHERE name = 'charging_stations';

-- Future manually-added stations continue with AA, AB, ... and receive ten piles.
CREATE TRIGGER trg_station_create_ten_piles
AFTER INSERT ON charging_stations
BEGIN
    INSERT INTO charging_piles
        (station_id, pile_number, pile_type, power_kw, status,
         total_charge_count, total_charge_duration)
    SELECT NEW.station_id,
           CASE
               WHEN NEW.station_id <= 26
                   THEN char(64 + NEW.station_id)
               ELSE char(64 + ((NEW.station_id - 1) / 26))
                    || char(65 + ((NEW.station_id - 1) % 26))
           END || '-' || printf('%02d', sequence.ordinal),
           CASE WHEN sequence.ordinal <= 6 THEN 'fast' ELSE 'slow' END,
           CASE WHEN sequence.ordinal <= 6 THEN 120.0 ELSE 7.0 END,
           'idle', 0, 0
    FROM (
        SELECT 1 AS ordinal UNION ALL SELECT 2 UNION ALL SELECT 3
        UNION ALL SELECT 4 UNION ALL SELECT 5 UNION ALL SELECT 6
        UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9
        UNION ALL SELECT 10
    ) AS sequence;
END;

CREATE TRIGGER trg_station_reject_eleventh_pile
BEFORE INSERT ON charging_piles
WHEN (SELECT COUNT(*) FROM charging_piles WHERE station_id = NEW.station_id) >= 10
 AND NOT EXISTS (SELECT 1 FROM charging_piles WHERE pile_id = NEW.pile_id)
 AND NOT EXISTS (
     SELECT 1 FROM charging_piles
     WHERE station_id = NEW.station_id AND pile_number = NEW.pile_number
 )
BEGIN
    SELECT RAISE(ABORT, 'each station may contain at most ten charging piles');
END;
