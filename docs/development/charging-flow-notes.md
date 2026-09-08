# 充电流程功能移交说明（UML-025~032）

> 分支：`feature/charging-flow`　状态：服务端 + 用户端已完成，虚拟机实测通过
> 协议契约全表见 `docs/development/shared-identifiers.md`「充电协议」一节

## 一、本次交付范围

| 端 | 内容 |
| --- | --- |
| 服务层 | `ev::ChargeService` 六方法：checkPending / checkPile / reserve / startCharge / endCharge / cancel |
| 服务端 | `ChargingSessionManager` 充电模拟（1 Hz 推送 + 落库）；`ServerApplication` 接入 0x30 分发 |
| 用户端 | `ChargeFlowWidget` 四态页（检查/确认/进度/结算）；`UserApiClient` 充电通道与 0x32 信号；找桩页"选择"接线 |

## 二、后续开发需遵守的规则

1. **金额一律整数分**（`*_cent` 字段），仅在展示层除以 100；电量单位为度（kWh）。
2. **订单归属以会话为准**：服务端从 `session_id` 反查 user_id，任何写操作（start/end/cancel）都校验 `order.user_id == 会话用户`，请勿在前端传 user_id 代替。
3. **桩状态流转固定**：`idle→reserved→in_use→idle`，全部经 `PileRepository::updateStatus` 乐观锁裁决，不要直接 UPDATE 桩状态字段。
4. **计费基于墙钟**：电量/费用按 `start_time` 到当前时刻计算，服务端每秒推送 0x32 并同步落库（订单页刷新即可看到实时数据）。
5. **`pending_settlement` 语义**：余额不足时订单停在 `pending_settlement`、桩保持 `in_use`；用户充值后再次调用 `end_charge` 即可重新结算。管理端做订单管理时请把该状态视为"待结算（占用中）"。
6. **断连不终止充电**：重连后用户端 `check_pending` 自动恢复进度页并重新绑定 0x32 推送。

## 三、协议速查

- 请求 `ChargeRequest(0x30)` / 响应 `ChargeResponse(0x31)`：`data = { type, params, session_id }`，结果在 `data.result`。
- 推送 `ChargeUpdate(0x32)`：无 `request_id`，字段 `{ order_id, charge_amount_kwh, current_fee_cent, progress }`，客户端经 `ev::UserApiClient::chargeUpdateReceived` 信号接收。
- 错误码：`STATE_CONFLICT`（并发预约冲突/状态不符）、`NOT_FOUND`（订单不存在或非本人）、`INSUFFICIENT_BALANCE`（余额不足，进入待结算）。

## 四、涉及文件（改动/新增）

**新增**：`src/services/charge_service.*`、`apps/server/charging_session_manager.*`、`apps/user_client/charge_flow_widget.*`

**修改**：`apps/server/server_application.*`、`apps/server/server.pro`、`src/services/services.pro`、`apps/user_client/user_api_client.*`、`user_home_widget.*`、`station_search_widget.*`、`user_client.pro`、`docs/development/shared-identifiers.md`

> 正在做管理端/订单页的组员：`server_application.cpp` 与 `services.pro` 有新增内容，合并时请以本分支为准 rebase，冲突集中在 include 列表与 `processFrame` 分发表。

## 五、验证路径

1. 构建 `ev_services` → `server` → `user_client`（顶层 `.pro` 会依次带上）。
2. 登录用户 → "找桩"页选空闲桩 → 自动跳"充电"页 → 预约 → 开始充电（观察进度条/电量/费用每秒刷新）→ 结束充电看结算。
3. 异常场景：余额不足充值后"重新结算"；充电中途断开客户端再重连（`check_pending` 恢复）；管理端订单页看 charging 订单实时电量与费用。
