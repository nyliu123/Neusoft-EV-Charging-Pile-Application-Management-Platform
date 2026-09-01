# 总体架构

## 1. 运行单元

```text
evcs_user_client  ─┐
                   ├─ TCP / length-prefixed JSON ─ evcs_server ─ SQLite (WAL)
evcs_admin_client ─┘                                  │
                                                     ├─ 统计 JSON ─ 本地 HTTP 桥接 ─ ECharts 大屏
                                                     └─ 只读预处理 ─ CSV + 数据质量报告
```

- 用户端和管理端不直接访问数据库。
- 服务端是业务规则和状态转换的唯一执行者。
- Socket 由主线程管理，业务请求进入有界线程池；每个任务建立自己的 SQLite 连接，禁止跨线程复用数据库连接。
- SQLite 采用单库、WAL、5 秒 busy timeout 和 `BEGIN IMMEDIATE` 事务；大屏只读取服务端统计结果。

## 2. 代码模块

- `src/common`：消息封装、拆包器、领域枚举和共享数据结构。
- `src/server`：Socket 会话、鉴权、业务服务、数据库仓储和定时任务。
- `src/user_client`：用户工作流和用户界面。
- `src/admin_client`：运营管理工作流和管理界面。
- `tests`：协议、数据库、业务规则和双客户端 Socket 并发测试。
- `dashboard`：离线 ECharts 页面、许可证和四类运营图表。
- `tools/dashboard_bridge.py`：只监听本机的 HTTP/TCP 协议桥接，不直接访问数据库。
- `tools/preprocess_analytics.py`：只读打开 SQLite，校验关联、时间与数值，导出 CSV 和 JSON 质量报告。

## 3. 关键状态机

### 充电桩

```text
空闲 → 已预约 → 充电中 → 空闲
  ├──────────────→ 故障
  ├──────────────→ 离线
  └──────────────→ 停用
```

### 预约

```text
有效 → 已使用
  ├──→ 已取消
  └──→ 已过期
```

### 订单

```text
充电中 → 待支付 → 已支付
                 └→ 已取消（仅异常恢复流程）
```

## 4. 安全与一致性

- 密码只保存加盐摘要，不保存明文。
- 除注册、登录和健康检查外，请求必须携带会话令牌。
- 修改充电桩占用状态时使用 SQLite `BEGIN IMMEDIATE` 事务。
- 请求使用唯一 `requestId`，响应原样返回，客户端可匹配并处理超时。
- 客户端请求 10 秒无响应即提示超时；连接断开时停止轮询并返回登录页。
- 服务端退出时先解除连接回调再销毁 Socket，避免析构期访问失效状态。
- 服务端退出前等待线程池任务结束；工作线程通过主线程队列回写 Socket。
- 日志不得记录密码、会话令牌或完整敏感个人信息。

## 5. 机器学习预留

服务端保留 `analytics.forecast` 与 `recommendation.list` 的协议动作，但在没有合格数据前返回 `FEATURE_NOT_READY`。模型不进入核心程序编译依赖。
