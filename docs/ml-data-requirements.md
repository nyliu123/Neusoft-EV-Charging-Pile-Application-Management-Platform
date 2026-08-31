# 机器学习功能预留与数据要求

## 1. 当前决定

负荷预测和智能推荐暂不训练、不展示伪准确率、不进入验收主流程。服务端保留：

- `system.features`：返回功能开关和停用原因；
- `analytics.forecast`：当前返回 `FEATURE_NOT_READY`；
- `recommendation.list`：当前返回 `FEATURE_NOT_READY`。

这三个接口可让客户端稳定处理“尚未具备数据条件”的状态，后续模型上线时无需改协议入口。

## 2. 负荷预测所需数据

建议按站点、15 分钟时间粒度连续采集至少 6 个月，最好覆盖 12 个月：

| 字段 | 说明 |
|---|---|
| `station_id`、`timestamp` | 站点和 UTC 时间窗口 |
| `energy_kwh`、`peak_power_kw` | 窗口充电量和峰值功率 |
| `active_chargers`、`available_chargers` | 设备使用和供给状态 |
| `session_count`、`average_duration_minutes` | 会话数量和平均时长 |
| `temperature_c`、`precipitation_mm`、`weather_code` | 可追溯来源的天气数据 |
| `weekday`、`is_holiday`、`special_event` | 日历和异常事件 |
| `outage_minutes`、`maintenance_minutes` | 停电、离线和维护影响 |

启用前门槛：时间覆盖率不低于 95%，时间戳和站点一致，异常停机有标记，训练/验证/测试按时间顺序切分，并与“昨日同刻”和“近 7 日均值”等基线比较。

## 3. 智能推荐所需数据

至少采集 10,000 次匿名查询—选择—到站结果，并覆盖不少于 20 个站点：

| 字段 | 说明 |
|---|---|
| `anonymous_user_id`、`query_id` | 不可反推身份的稳定匿名标识 |
| `query_time`、`origin_grid` | 查询时间和模糊位置网格 |
| `candidate_station_ids` | 当时实际返回的候选集合 |
| `distance_km`、`estimated_wait_minutes`、`price_cents_per_kwh` | 候选特征快照 |
| `selected_station_id` | 用户选择；未选择也要记录 |
| `arrived`、`charged`、`wait_minutes` | 到站和最终充电结果 |
| `station_fault_or_offline` | 推荐后不可用等异常原因 |

不得采集明文姓名、手机号或精确长期轨迹。位置只保留完成推荐所需的最小网格精度，并制定保留期限。

## 4. 上线验收条件

1. 数据字典、来源、授权和质量报告经过确认。
2. 离线评估优于简单规则基线，且给出置信区间。
3. 负荷预测至少报告 MAE、RMSE、MAPE；低负荷窗口单独说明 MAPE 失真风险。
4. 推荐至少报告命中率、到站转化、平均等待时间和价格/距离公平性护栏。
5. 模型版本、训练数据时间范围、特征版本和回滚方式可追踪。
6. 先影子运行，再小流量试用；没有真实效果证据前不自动改变充电或价格策略。
