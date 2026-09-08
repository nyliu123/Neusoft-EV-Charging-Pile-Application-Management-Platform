# 共享标识符清单

本清单是跨分支公共名称的唯一登记入口。修改名称或签名时，必须在同一次合并中更新所有调用方与本清单。

| 标识符                                       | 所属域  | 唯一定义位置                                  | 用途                           |
| ----------------------------------------- | ---- | --------------------------------------- | ---------------------------- |
| `ev::ProtocolVersion`                     | 通信   | `src/common/protocol.h`                 | 当前协议版本                       |
| `ev::MaxPayloadBytes`                     | 通信   | `src/common/protocol.h`                 | 单帧正文最大 10 MiB                |
| `ev::MessageType`                         | 通信   | `src/common/protocol.h`                 | TCP 消息类型编号                   |
| `ev::ErrorCode`                           | 公共   | `src/common/error_code.h`               | 统一业务错误码                      |
| `ev::Result<T>`                           | 公共   | `src/common/result.h`                   | 服务统一返回结构                     |
| `ev::PhoneValidator`                      | 用户   | `src/common/phone_validator.h`          | 客户端和服务端共用手机号格式校验             |
| `ev::PasswordHash` / `ev::PasswordHasher` | 安全   | `src/common/password_hasher.h`          | PBKDF2-SHA256 密码哈希与验证        |
| `ev::Frame`                               | 通信   | `src/network/frame_codec.h`             | 已解析消息帧                       |
| `ev::FrameCodec`                          | 通信   | `src/network/frame_codec.h`             | 8 字节头加 JSON 正文编解码            |
| `ev::PlatformClient`                      | 通信   | `src/network/platform_client.h`         | 客户端连接、健康检查与重连、会话管理           |
| `ev::FeeCalculator`                       | 充电   | `src/services/fee_calculator.h`         | 按订单快照计算整数分费用                 |
| `ev::DatabaseManager`                     | 数据   | `src/data/database_manager.h`           | 每线程 SQLite 连接与迁移入口           |
| `ev::UserRepository`                      | 用户数据 | `src/data/user_repository.h`            | 用户查询、资料更新和余额事务             |
| `ev::UserService`                         | 用户服务 | `src/services/user_service.h`           | UML-011～017 用户登录、注册与个人信息   |
| `ev::StationRepository`                   | 站点数据 | `src/data/station_repository.h`         | 站点、设备只读查询与统计               |
| `ev::StationService`                      | 站点服务 | `src/services/station_service.h`        | 需求22～26列表、距离排序与详情          |
| `ev::MapApiAdapter`                       | 外部接口 | `src/adapters/map_api_adapter.h`        | OpenStreetMap 地址解析与离线教学坐标兜底 |
| `ev::ChargeService`                       | 充电服务 | `src/services/charge_service.h`         | UML-025~032 预约、充电与结算业务闭环     |
| `ev::ChargingSessionManager`              | 服务端  | `apps/server/charging_session_manager.h`| 充电模拟会话与 0x32 实时推送           |
| `ev::SessionManager`                      | 用户服务 | `src/services/session_manager.h`        | UML-013 服务端内存会话、续期与清理        |
| `ev::AdminInfo` / `ev::AdminAuthService`  | 认证   | `src/services/admin_auth_service.h`     | 管理员身份认证服务                    |
| `ev::AdminSeeder`                         | 认证   | `src/services/admin_seeder.h`           | 默认管理员账号初始化                   |
| `UserSessionState`                        | 用户端  | `apps/user_client/user_session_state.h` | 保存当前登录用户与会话标识                |
| `ev::UserApiClient`                       | 用户端  | `apps/user_client/user_api_client.h`    | 关联用户请求与异步响应                  |
| `ChargeFlowWidget`                        | 用户端  | `apps/user_client/charge_flow_widget.h` | 充电流程界面（检查/选桩/进度/结算四态页）  |
| `ev::AdminSession`                        | 管理端  | `apps/admin_client/admin_session.h`     | 管理端登录会话信息                    |

新增消息类型必须显式分配未使用编号，并同步更新客户端、服务端与协议测试；禁止依据枚举顺序隐式生成线上编号。

主干保留 `HealthRequest=0x03` 和 `HealthResponse=0x04` 作为最小运行检查。业务分支不得改变其语义；健康检查只确认协议和服务状态，不执行登录或业务写入。

## 登录协议（LoginRequest/LoginResponse）

### 管理员登录（role=admin）

请求 data 字段：

```json
{
  "username": "admin",
  "password": "admin123",
  "role": "admin"
}
```

响应 data 字段（成功时）：

```json
{
  "admin_id": 1,
  "username": "admin",
  "session_id": "uuid"
}
```

`session_id` 由服务端 `SessionManager` 注册（userId 存 adminId），客户端登录成功后必须调用
`PlatformClient::activateSession(session_id)` 启动心跳；后续管理端所有请求都要携带该
`session_id`，服务端校验失败返回 `UNAUTHORIZED`。

### 用户登录（role=user 或不传）

请求 data 字段：

```json
{
  "phone": "13800138000",
  "is_auto_register": false
}
```

响应 data 字段（成功时）：

```json
{
  "user_info": {
    "user_id": 1,
    "nickname": "...",
    "avatar_path": "...",
    "balance": 100.00,
    "session_id": "..."
  },
  "is_new_user": false
}
```

- 用户名不存在与密码错误均返回 `UNAUTHORIZED`，避免用户枚举

- `role` 字段：`admin` 表示管理员登录，不传或 `user` 表示普通用户登录

## 用户信息协议（UserRequest=0x10 / UserResponse=0x11）

用户登录后的个人信息业务复用一对消息类型，请求 `data` 统一为
`{ type, params, session_id }`，响应统一为 `{ success, code, message, data: { result } }`。

| type | params | result | 对应设计 |
| ---- | ------ | ------ | -------- |
| `user_info` | 无 | user_id / nickname / avatar_path / balance_cent | UML-014 |
| `update_avatar` | file_data（Base64 JPG，最大2 MiB） | avatar_path | UML-015 |
| `update_nickname` | nickname | nickname | UML-016 |
| `recharge` | amount_cent（1～999999） | balance_cent | UML-017 |

服务端根据 `session_id` 反查用户，不信任客户端传入的 `user_id`。账号冻结或会话
失效时客户端立即清理本地会话并返回登录页。充值金额全链路使用整数分。

## 站点协议（StationRequest=0x20 / StationResponse=0x21）

请求 `data` 统一为 `{ type, params, session_id }`，服务端只接受当前连接上有效的用户会话；响应结果位于 `data.result`。

| type | params | result | 对应需求 |
| ---- | ------ | ------ | -------- |
| `geocode` | address | longitude / latitude / display_address / source / confidence | 22、23、67 |
| `station_list` | longitude / latitude（同时提供或同时省略） | stations，含价格、总桩数、空闲数、rating 评分摘要；有位置时含直线距离 | 24、25 |
| `station_detail` | station_id | station（含 rating）/ piles / stats，未知状态绝不按空闲返回 | 26 |
| `list_comments` | station_id | summary（平均分/档位/条数）/ comments（含 like_count、liked_by_me、is_mine），热评在前 | 新增：评论 |
| `post_comment` | station_id / content（1~200 字）/ rating（0~5 星） | comment_id / updated（一人一站一条，重复即修改） | 新增：评论 |
| `toggle_like` | comment_id | liked / like_count（UNIQUE 去重，可反复切换） | 新增：评论 |

地址解析默认使用 OpenStreetMap Nominatim，并对有限的大连预设区域返回明确标识的教学坐标；地图不可用时客户端可不带经纬度查询文字列表，`distance_km` 必须缺省。

### 评论与评分（站点协议扩展）

评分摘要 `rating` 附在 `station_list` 的每个站对象和 `station_detail` 的 `station`
对象上：`{ avg, count, tier, hot }`，无评论时 avg/tier/hot 为 null、count 为 0。
`avg` 为 0~10 的一维小数（星 ×2）；`tier` 档位固定五档：`≥9 夯 / ≥7 顶级 /
≥5 人上人 / ≥3 拉 / <3 拉完了`；`hot` 为点赞最高的评论 `{ nickname, content }`。

评论数据存于 `station_comments`（UNIQUE(station_id, user_id)）与 `comment_likes`
（UNIQUE(comment_id, user_id)）两张表，由 `DatabaseManager::migrate()` 幂等建表，
旧运行库启动即自动升级，无需改动 qrc 模板。热评排序规则：点赞数降序、
created_at 升序。演示数据可用 `python3 scripts/seed_comments.py` 一键生成。

## 充电协议（ChargeRequest=0x30 / ChargeResponse=0x31 / ChargeUpdate=0x32）

UML-025~032 充电业务闭环复用一对请求/响应消息类型，`data` 统一为
`{ type, params, session_id }`，响应结果位于 `data.result`；服务端只接受当前连接上
有效的用户会话，订单归属一律以会话反查的 user_id 为准，不信任客户端参数。

### type 清单（ChargeRequest）

| type | params | result | 对应设计 |
| ---- | ------ | ------ | -------- |
| `check_pending` | 无 | has_pending / order（含桩、站点、单价；charging 时附带 live 实时快照并重新绑定推送） | UML-025 |
| `check_pile` | pile_id | available / reason / pile / station_name / price_per_kwh | UML-026 |
| `reserve` | pile_id | order_id / pile / station_name / price_per_kwh | UML-027 |
| `start_charge` | order_id | order_id / start_time / power_kw / price_per_kwh | UML-028 |
| `end_charge` | order_id | settled / order_id / total_kwh / total_fee_cent / balance_cent / shortfall_cent | UML-031 |
| `cancel_charge` | order_id | order_id | UML-027（取消预约） |

### 服务端推送（ChargeUpdate=0x32）

订单进入 charging 后，服务端 `ChargingSessionManager` 以 1 Hz 按墙钟时间计算
`kwh = power_kw × elapsed / 3600`、`fee = kwh × price_per_kwh`（订单快照单价）并推送：

```json
{
  "protocol_version": 1,
  "order_id": 1,
  "charge_amount_kwh": 0.83,
  "current_fee_cent": 125,
  "progress": 1.67
}
```

该帧无 `request_id`，客户端 `ev::UserApiClient::chargeUpdateReceived` 信号分发。
进度按"满充量 50 度"假设计算（UML-029）。

### 关键规则

- 金额全链路整数分（`*_cent`），展示层除以 100；电量单位为度（kWh）。
- reserve 在同一事务内完成"桩 idle→reserved + 订单插入（单价快照）"；
  并发预约由乐观锁裁决，冲突方收到 `STATE_CONFLICT`。
- 余额充足时结算在同一事务内完成"扣余额 + 订单 settled + 桩 idle + 桩累计统计"；
  余额不足时订单进入 `pending_settlement`，桩保持 `in_use`，用户充值后再次
  `end_charge` 即可重新结算。
- 断连不终止充电：订单按墙钟继续计费，重连后 `check_pending` 恢复进度页并
  重新绑定 0x32 推送。

## 管理端数据协议（AdminQuery=0x60 / AdminAction=0x61 / AdminResponse=0x62）

管理员登录以外的管理端业务全部复用这三个消息类型，用 `type` 字符串区分具体请求，
不再新增消息编号。处理逻辑集中在服务端 `ev::AdminHandler`（`apps/server/admin_handler.h`）。

### 请求格式（查询与操作相同）

```json
{
  "protocol_version": 1,
  "request_id": "uuid",
  "data": {
    "type": "pile_list",
    "params": { "station_id": 1 },
    "session_id": "登录时获得的会话标识"
  }
}
```

### 响应格式

```json
{
  "protocol_version": 1,
  "request_id": "同请求",
  "success": true,
  "code": "OK",
  "message": "",
  "data": {
    "type": "回显请求 type",
    "result": { "...": "按 type 定义的查询结果或操作结果" }
  }
}
```

会话校验失败时：`success=false`、`code=UNAUTHORIZED`、
`data.error="session_expired"`，客户端应跳转登录页。

### 查询 type 清单（AdminQuery）

| type                 | params                          | result 内容                     | 对应设计 |
| -------------------- | ------------------------------- | ----------------------------- | ------ |
| `dashboard_summary`  | 无                              | total_kwh / total_revenue / total_users / total_orders | UML-035 |
| `revenue_trend`      | days（默认 7，1~90）              | points: [{date, revenue}]，按天零填充 | UML-036 |
| `pile_status_stats`  | 无                              | stats: [{status, label, count}]，四种状态零填充 | UML-037 |
| `pile_list`          | station_id / status（均可选）     | piles: [{pile_id, pile_number, station_id, station_name, pile_type, power_kw, status, total_charge_count, total_charge_duration}] | UML-038 |
| `station_list`       | 无                              | stations: [{station_id, station_name, address, longitude, latitude, price_per_kwh, pile_count}] | UML-040 |
| `station_detail`     | station_id                      | station + piles（一次返回站点与桩）  | UML-041 |
| `user_list`          | keyword（可选，手机号模糊匹配）    | users: [{user_id, phone, nickname, balance, register_time, status}] | UML-043 |
| `order_list`         | status / station_id / start_date / end_date（均可选） | orders: [{order_id, status, reserve_time, start_time, end_time, charge_amount_kwh, price_per_kwh, total_fee, user_phone, user_nickname, pile_number, pile_type, station_name}] | UML-046 |

### 操作 type 清单（AdminAction）

| type             | params                                              | 说明                                | 对应设计 |
| ---------------- | --------------------------------------------------- | --------------------------------- | ------ |
| `restart_pile`   | pile_id                                             | 仅故障桩可重启，成功后状态恢复 idle      | UML-039 |
| `add_station`    | station_name / address / longitude / latitude / price_per_kwh | 校验后插入，返回 station_id      | UML-042 |
| `set_user_status`| user_id / status（`frozen` 或 `normal`）              | 冻结会同时踢掉该用户全部在线会话        | UML-045 |

### 管理端复用站点协议的 geocode

管理端“新增充电站”（UML-042）的地址解析复用站点协议的 `StationRequest=0x20 / type="geocode"`，
不新增消息编号：服务端 geocode 校验的是“会话绑定本连接 + SessionManager 有效”，管理员会话同样满足。
管理端客户端 `ev::AdminApiClient::sendGeocode()` 发送该请求并按 `StationResponse=0x21`
（信封与 AdminResponse 完全一致）解析；地理编码失败时表单自动解锁手动填写经纬度。

## 共享测试数据

`data/ev_charging.sqlite3` 是全组共享并随 Git 提交的演示数据库，包含 26 个站点、
260 台桩（覆盖四种状态）、5 个用户（含冻结账号 15812349876）和 15 笔订单。
用户端与管理端联调请直接使用其中的演示数据。服务端在显式指定一个不存在的
数据库路径时，会从这个内置模板创建独立副本。

## 个人订单查询（UML-033 / 需求38）

- `ev::OrderRecord` / `ev::OrderRepository::findByUser`：复用 `src/data/order_repository.h` 的共享订单模型，关联查询本人订单（金额采用 `totalFeeCent`）。
- `ev::OrderService::queryOrders(database, authenticatedUserId)`：`src/services/order_service.h`，校验账号、映射状态和计算完整起止时间的时长。
- `ev::UserApiClient::queryOrders(context, callback)`：用户端异步查询入口。
- `OrderListWidget`：`apps/user_client/order_list_widget.h`，订单卡片、详情、刷新和返回首页。

详细设计的 `QUERY_ORDERS_REQ/RESP` 在现有 TCP 协议中映射为
`UserRequest=0x10 / UserResponse=0x11`，请求 `data={type:"query_orders",params:{},session_id}`，
不新增消息编号。服务端以当前连接绑定的用户会话反查身份，忽略任何客户端 `user_id`；
管理员会话、过期会话、冻结账号均不得查询个人订单。

成功结果为 `data.result.orders` 数组，每项包含 `order_id, station_name, pile_number,
status, status_text, charge_amount_kwh, price_per_kwh, total_fee, reserve_time,
start_time, end_time, duration_hours`。按 `reserve_time DESC, order_id DESC` 排序；
金额、单价、电量均来自订单记录，不按站点当前价格重新计算。缺失起止时间返回 `null`；
仅当两者有效且结束不早于开始时返回小时数，否则时长为 `null`，不按当前时间推算。
当前订单表尚无会员优惠快照字段，查询不推算或伪造优惠明细，待会员/结算模块提供后扩展。

客户端每次进入“我的订单”重新查询；15 秒未响应可重试，离开页面或会话失效会清空卡片并丢弃旧响应。
待结算订单仅提示前往充电流程处理，不修改状态或扣款。

## 内嵌地图导航（UML-023～024）

- `NavigationMapDialog`：`apps/user_client/navigation_map_dialog.h`，由 Qt 请求 Valhalla 路线，
  在独立窗口的 `QWebEngineView` 中绘制精简 OpenStreetMap 瓦片、路线和起终点。
- `NavigationMapDialog::directionsUrl(...)`：统一校验坐标并生成 `auto`、`pedestrian`
  或 `bicycle` 路线请求。

站点详情的导航按钮只创建应用内地图窗口，不调用系统浏览器。页面不加载 OpenStreetMap 网站菜单，只显示地图、
纵向路线详情、起终点地址和版权署名。地图支持左键拖拽和滚轮缩放，右下角比例尺、`+/-`、
缩放百分比与重置按钮同步，放大百分比不设固定上限；比例尺达到最小值 5 米时停止继续放大。
切换驾车、步行或骑行方式会取消旧请求并丢弃迟到结果；
长距离步行和骑行会自动分段请求后合并。20 秒超时、网络错误、无路线或数据不完整时显示明确错误。
地图窗口关闭或用户会话失效时停止显示，且不修改业务数据。

验证：完整构建后运行 `bin/ev_unit_tests`、
`QT_QPA_PLATFORM=offscreen bin/ev_order_ui_tests`（含15秒超时重试检查）和
`python3 tests/integration/order_query.py <构建目录>`；后者使用临时数据库与独立 TCP 端口。
