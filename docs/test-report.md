# 第一阶段测试报告

## 1. 测试结论

截至 2026-08-31，第一阶段核心系统在 BitDev 虚拟机内完成干净构建，4 个自动化测试全部通过；服务端、用户端和管理端均通过进程级启动测试，Socket 完整业务链路与 JSON 结构化日志验证通过。

## 2. 测试环境

- VMware Workstation 17.5.2
- Ubuntu 22.04.3 LTS（BitDev）
- Qt 6.2.4，Qt Creator 6.0.2
- GCC/G++ 11.4.0
- CMake 3.22.1
- SQLite Qt 驱动，WAL 模式

说明：Qt 运行库满足项目要求；当前 Qt Creator 为 6.0.2，低于说明书建议的 6.2，但不影响 CMake 构建和程序运行。最终课堂环境如严格检查 IDE 版本，可单独升级 Qt Creator。

## 3. 自动化测试

| 测试 | 覆盖内容 | 结果 |
|---|---|---|
| `protocol_test` | 长度前缀 JSON 拆包、分段帧、超限帧拒绝 | 通过 |
| `business_flow_test` | 注册/登录、个人信息、退出、站点、预约、充电、计费、订单、管理员 CRUD、7/30 日统计 | 通过 |
| `socket_integration_test` | 两个真实 TCP 客户端登录并同时预约同一充电桩，仅一个成功 | 通过 |
| `ui_acceptance_test` | 两端真实连接、登录、数据加载、界面渲染和截图 | 通过 |

最近一次完整结果：4/4 通过，总耗时约 1.76 秒。原始输出保存在宿主机 `D:\学生资料\.codex-tmp\vm-control\evcs-build-023-report.txt`。

ECharts 第二阶段完成后再次从干净目录构建，4/4 测试通过，总耗时约 1.91 秒；原始输出为 `D:\学生资料\.codex-tmp\vm-control\evcs-build-025-report.txt`。

最终 1.0.0 发布候选从全新目录完成构建、4/4 测试、大屏端到端测试和源码包校验，总测试耗时约 1.84 秒；完整记录为 `D:\学生资料\.codex-tmp\vm-control\evcs-final-035-report.txt`。

## 4. 端到端验证

已通过 TCP Socket 完成：

`登录 → 查询站点 → 创建预约 → 使用预约开始充电 → 查询实时状态 → 结束充电并结算 → 查询订单`

验证结果包括 3 个种子站点、有效预约、持续增长的充电时间/电量、结算订单和已支付状态。原始输出保存在 `D:\学生资料\.codex-tmp\vm-control\evcs-smoke-013-report.txt`。

## 5. 稳定性验证

- 用户端和管理端均能在 Ubuntu 的 Qt `offscreen` 平台持续运行，测试时由超时工具正常结束，不是程序崩溃。
- GUI 验收测试生成 `docs/images/user-client-acceptance.png` 和 `docs/images/admin-client-acceptance.png`，已人工检查布局与数据渲染。
- 客户端断线后停止充电状态轮询、清理令牌并返回登录页。
- 请求 10 秒无响应会返回 `REQUEST_TIMEOUT`，不会永久卡住界面。
- 服务端捕获未知异常并返回 `INTERNAL_ERROR`，日志不记录密码或会话令牌。
- 双客户端抢占同一充电桩时事务约束生效。
- 测试曾发现服务端和客户端析构阶段的 Socket 生命周期缺陷，已使用 AddressSanitizer 定位，通过显式解除连接回调修复并加入回归测试。

## 6. 当前非缺陷限制

- 充电硬件、支付和余额均为教学模拟，不连接真实设备或支付渠道。
- 机器学习因无合格真实数据暂缓；接口返回 `FEATURE_NOT_READY`。
- ECharts 6.1.0 已离线集成；统计接口、HTTP 桥接、105 条幂等演示订单和四张图表端到端通过。原始输出为 `D:\学生资料\.codex-tmp\vm-control\evcs-dashboard-026-report.txt`，渲染截图为 `docs/images/dashboard-acceptance.png`。
