# Socket 通信协议

## 1. 传输层

- TCP，默认端口 `8888`。
- 每个消息由 4 字节无符号大端消息类型、4 字节无符号大端长度和 UTF-8 JSON 负载组成。
- 单个 JSON 负载最大 1 MiB；超过限制时断开连接并记录协议错误。

```text
+----------------------+----------------------+-------------------------+
| messageType (u32)    | payloadLength (u32)  | JSON payload (UTF-8)    |
+----------------------+----------------------+-------------------------+
```

消息类型取值：`1=request`、`2=response`、`3=event`。消息头类型必须与 JSON 的 `type` 字段一致。

## 2. 请求

```json
{
  "type": "request",
  "requestId": "uuid",
  "action": "station.list",
  "token": "optional-session-token",
  "payload": {}
}
```

## 3. 成功响应

```json
{
  "type": "response",
  "requestId": "uuid",
  "ok": true,
  "data": {}
}
```

## 4. 错误响应

```json
{
  "type": "response",
  "requestId": "uuid",
  "ok": false,
  "error": {
    "code": "INVALID_ARGUMENT",
    "message": "可展示给用户的简短说明"
  }
}
```

## 5. 第一阶段动作

- `system.ping`
- `system.features`
- `auth.register`、`auth.login`、`auth.phoneLogin`、`auth.phoneRegister`、`auth.logout`
- `user.profile`、`user.profile.update`、`user.avatar.update`
- `wallet.recharge`
- `station.list`、`station.get`
- `reservation.create`、`reservation.cancel`、`reservation.list`
- `charging.start`、`charging.status`、`charging.stop`；充电中每秒推送 `charging.update` 事件
- `order.list`、`order.pending`、`order.get`
- `admin.dashboard`
- `admin.analytics`
- `admin.demo.generateHistory`（必须传入 `confirmed: true`）
- `admin.station.list`、`admin.station.save`
- `admin.charger.list`、`admin.charger.save`、`admin.charger.setStatus`
- `admin.charger.restart`、`admin.charger.operation.list`
- `admin.user.list`、`admin.user.setStatus`
- `admin.reservation.list`、`admin.session.list`、`admin.order.list`
- `admin.tariff.list`、`admin.tariff.save`
- `admin.fault.list`、`admin.fault.save`
- `map.geocode`

## 6. 错误码

- `INVALID_MESSAGE`、`INVALID_ARGUMENT`
- `UNAUTHENTICATED`、`FORBIDDEN`
- `NOT_FOUND`、`CONFLICT`
- `CHARGER_UNAVAILABLE`、`RESERVATION_EXPIRED`
- `DATABASE_ERROR`、`INTERNAL_ERROR`
- `FEATURE_NOT_READY`
- `CONFIRMATION_REQUIRED`
- `USER_NOT_FOUND`、`DUPLICATE_REQUEST`、`REQUEST_TIMEOUT`

## 7. 稳定性规则

- 客户端断线后每 2 秒自动重连，会话令牌保留在内存中；重连成功后自动刷新业务数据。
- 客户端请求 10 秒未响应会报告 `REQUEST_TIMEOUT`。
- 服务端按“会话令牌 + requestId”缓存已完成响应；相同请求不会重复执行写操作。
- 非法长度、未知消息类型、消息头与 JSON 类型不一致都会被拒绝。
