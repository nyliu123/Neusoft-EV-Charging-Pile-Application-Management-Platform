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
  "username": "admin"
}
```

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

