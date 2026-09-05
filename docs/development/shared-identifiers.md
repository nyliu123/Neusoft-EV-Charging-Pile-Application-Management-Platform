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
| `ev::UserRepository`                      | 用户数据 | `src/data/user_repository.h`            | 按手机号读取已有用户记录                 |
| `ev::UserService`                         | 用户服务 | `src/services/user_service.h`           | UML-011 已有用户登录与 UML-012 自动注册 |
| `ev::SessionManager`                      | 用户服务 | `src/services/session_manager.h`        | UML-013 服务端内存会话、续期与清理        |
| `ev::AdminInfo` / `ev::AdminAuthService`  | 认证   | `src/services/admin_auth_service.h`     | 管理员身份认证服务                    |
| `ev::AdminSeeder`                         | 认证   | `src/services/admin_seeder.h`           | 默认管理员账号初始化                   |
| `UserSessionState`                        | 用户端  | `apps/user_client/user_session_state.h` | 保存当前登录用户与会话标识                |
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

## 共享测试数据（003_seed_test_data.sql）

`resources/database/migrations/003_seed_test_data.sql` 提供全组共享的演示数据，启动服务端时
自动写入（固定主键 + INSERT OR IGNORE，可重复执行）：3 个站点、10 台桩（覆盖四种状态）、
5 个用户（含冻结账号 15812349876）、15 笔订单（近 7 日已结算订单可驱动看板营收趋势）。
用户端与管理端联调请直接使用这批手机号。删除 sqlite 文件并重启服务端即可重置。

**迁移编号占用：`003` 已被测试数据占用，组员新增迁移请从 `004` 开始。**

