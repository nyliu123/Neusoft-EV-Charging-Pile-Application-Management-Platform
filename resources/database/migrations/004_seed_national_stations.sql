-- Nationwide teaching/demo station distribution.
-- Counts are intentionally tiered (4-12 generated stations per provincial-level
-- region) to approximate differences in economic scale and charging demand; they
-- are not representations of real operators or current infrastructure totals.

-- New stations created later from the admin client receive ten piles
-- immediately, so the invariant is not limited to the initial seed data.
CREATE TRIGGER IF NOT EXISTS trg_station_create_ten_piles
AFTER INSERT ON charging_stations
BEGIN
    INSERT INTO charging_piles
        (station_id, pile_number, pile_type, power_kw, status,
         total_charge_count, total_charge_duration)
    VALUES
        (NEW.station_id, 'AUTO-01', 'fast', 120.0, 'idle',    0, 0),
        (NEW.station_id, 'AUTO-02', 'fast', 120.0, 'idle',    0, 0),
        (NEW.station_id, 'AUTO-03', 'fast', 120.0, 'idle',    0, 0),
        (NEW.station_id, 'AUTO-04', 'fast', 120.0, 'idle',    0, 0),
        (NEW.station_id, 'AUTO-05', 'fast', 120.0, 'idle',    0, 0),
        (NEW.station_id, 'AUTO-06', 'fast', 120.0, 'in_use',  0, 0),
        (NEW.station_id, 'AUTO-07', 'slow',   7.0, 'in_use',  0, 0),
        (NEW.station_id, 'AUTO-08', 'slow',   7.0, 'reserved',0, 0),
        (NEW.station_id, 'AUTO-09', 'slow',   7.0, 'fault',   0, 0),
        (NEW.station_id, 'AUTO-10', 'slow',   7.0, 'fault',   0, 0);
END;

CREATE TRIGGER IF NOT EXISTS trg_station_reject_eleventh_pile
BEFORE INSERT ON charging_piles
WHEN (SELECT COUNT(*) FROM charging_piles
      WHERE station_id = NEW.station_id) >= 10
 AND NOT EXISTS (
     SELECT 1 FROM charging_piles existing
     WHERE existing.pile_id = NEW.pile_id
        OR (existing.station_id = NEW.station_id
            AND existing.pile_number = NEW.pile_number)
 )
BEGIN
    SELECT RAISE(ABORT, 'a charging station can contain exactly ten piles');
END;

WITH RECURSIVE
province_plan(province, city, station_count, longitude, latitude, base_price) AS (
    VALUES
        ('广东省',       '广州市', 12, 113.2644, 23.1291, 1.35),
        ('江苏省',       '南京市', 12, 118.7969, 32.0603, 1.30),
        ('山东省',       '济南市', 11, 117.1201, 36.6512, 1.20),
        ('浙江省',       '杭州市', 11, 120.1551, 30.2741, 1.40),
        ('河南省',       '郑州市', 10, 113.6254, 34.7466, 1.15),
        ('四川省',       '成都市', 10, 104.0665, 30.5723, 1.18),
        ('湖北省',       '武汉市', 10, 114.3054, 30.5931, 1.20),
        ('福建省',       '福州市', 10, 119.2965, 26.0745, 1.28),
        ('湖南省',       '长沙市', 10, 112.9388, 28.2282, 1.18),
        ('安徽省',       '合肥市', 10, 117.2272, 31.8206, 1.18),
        ('上海市',       '浦东新区', 10, 121.4737, 31.2304, 1.55),
        ('北京市',       '朝阳区', 10, 116.4074, 39.9042, 1.55),
        ('河北省',       '石家庄市', 10, 114.5149, 38.0428, 1.15),
        ('陕西省',       '西安市',  9, 108.9398, 34.3416, 1.18),
        ('江西省',       '南昌市',  9, 115.8582, 28.6829, 1.15),
        ('重庆市',       '渝北区',  9, 106.5516, 29.5630, 1.20),
        ('辽宁省',       '沈阳市',  9, 123.4315, 41.8057, 1.18),
        ('云南省',       '昆明市',  8, 102.8329, 24.8801, 1.12),
        ('广西壮族自治区', '南宁市', 8, 108.3669, 22.8170, 1.12),
        ('山西省',       '太原市',  8, 112.5489, 37.8706, 1.12),
        ('内蒙古自治区', '呼和浩特市', 8, 111.7492, 40.8426, 1.10),
        ('贵州省',       '贵阳市',  8, 106.6302, 26.6477, 1.10),
        ('新疆维吾尔自治区', '乌鲁木齐市', 8, 87.6168, 43.8256, 1.08),
        ('天津市',       '滨海新区', 8, 117.3616, 39.3434, 1.28),
        ('黑龙江省',     '哈尔滨市', 7, 126.6424, 45.7560, 1.12),
        ('吉林省',       '长春市',  7, 125.3235, 43.8171, 1.12),
        ('甘肃省',       '兰州市',  6, 103.8343, 36.0611, 1.08),
        ('海南省',       '海口市',  6, 110.1983, 20.0440, 1.20),
        ('宁夏回族自治区', '银川市', 6, 106.2309, 38.4872, 1.08),
        ('青海省',       '西宁市',  4, 101.7782, 36.6171, 1.05),
        ('西藏自治区',   '拉萨市',  4,  91.1409, 29.6456, 1.05),
        ('香港特别行政区', '香港岛',  8, 114.1694, 22.3193, 1.60),
        ('澳门特别行政区', '澳门半岛', 4, 113.5439, 22.1987, 1.55),
        ('台湾省',       '台北市',  8, 121.5654, 25.0330, 1.45)
),
station_seq(n) AS (
    VALUES(1)
    UNION ALL SELECT n + 1 FROM station_seq WHERE n < 12
)
INSERT INTO charging_stations
    (station_name, address, longitude, latitude, price_per_kwh)
SELECT
    province || city || '示范站' || printf('%02d', n),
    province || city || '示范路' || n || '号',
    longitude + (((n - 1) % 4) - 1.5) * 0.025,
    latitude + (((n - 1) / 4) - 1.0) * 0.020,
    base_price + ((n - 1) % 3) * 0.05
FROM province_plan
CROSS JOIN station_seq
WHERE n <= station_count
  AND NOT EXISTS (
      SELECT 1 FROM charging_stations existing
      WHERE existing.station_name =
            province || city || '示范站' || printf('%02d', n)
  );

-- Fill every station that has fewer than ten piles without replacing existing
-- pile IDs or breaking orders that reference the original demo piles.
WITH RECURSIVE pile_seq(n) AS (
    VALUES(1)
    UNION ALL SELECT n + 1 FROM pile_seq WHERE n < 20
),
station_counts AS (
    SELECT s.station_id, COUNT(p.pile_id) AS existing_count
    FROM charging_stations s
    LEFT JOIN charging_piles p ON p.station_id = s.station_id
    GROUP BY s.station_id
),
available_numbers AS (
    SELECT counts.station_id, counts.existing_count, pile_seq.n,
           ROW_NUMBER() OVER (
               PARTITION BY counts.station_id ORDER BY pile_seq.n
           ) AS missing_rank
    FROM station_counts counts
    CROSS JOIN pile_seq
    WHERE NOT EXISTS (
        SELECT 1 FROM charging_piles existing
        WHERE existing.station_id = counts.station_id
          AND existing.pile_number = printf('AUTO-%02d', pile_seq.n)
    )
)
INSERT INTO charging_piles
    (station_id, pile_number, pile_type, power_kw, status,
     total_charge_count, total_charge_duration)
SELECT
    station_id,
    printf('AUTO-%02d', n),
    CASE WHEN n <= 6 THEN 'fast' ELSE 'slow' END,
    CASE WHEN n <= 6 THEN 120.0 ELSE 7.0 END,
    CASE
        WHEN n <= 5 THEN 'idle'
        WHEN n <= 7 THEN 'in_use'
        WHEN n = 8 THEN 'reserved'
        ELSE 'fault'
    END,
    0,
    0
FROM available_numbers
WHERE existing_count < 10
  AND missing_rank <= 10 - existing_count;
