# 第一阶段测试报告

## 1. 测试结论

截至 2026-09-01，1.1.0 发布候选在 BitDev 虚拟机内完成干净构建，5 个自动化测试全部通过；手机号登录、个人资料、地图找桩、管理端增强、线程池并发、数据预处理及旧数据库迁移均有回归验证。

## 2. 测试环境

- VMware Workstation 17.5.2
- Ubuntu 22.04.3 LTS（BitDev）
- Qt 6.2.4，Qt Creator 20.0.1
- GCC/G++ 11.4.0
- CMake 3.22.1
- SQLite Qt 驱动，WAL 模式

说明：Qt Creator 使用 Qt 官方 20.0.1 CPack 软件包安装，安装包 SHA-256 已与官方发布页核对；项目本身使用 Ubuntu 系统 Qt 6.2.4 编译。

## 3. 自动化测试

| 测试 | 覆盖内容 | 结果 |
|---|---|---|
| `protocol_test` | 长度前缀 JSON 拆包、分段帧、超限帧拒绝 | 通过 |
| `business_flow_test` | 核心充电流程、手机号自动注册、头像/昵称/充值、距离排序、站点设备联动、筛选、重启日志、7/30 日统计和 1.0 数据库迁移 | 通过 |
| `socket_integration_test` | 两个真实 TCP 客户端登录并同时预约同一充电桩，仅一个成功 | 通过 |
| `ui_acceptance_test` | 两端真实连接、登录、数据加载、界面渲染和截图 | 通过 |
| `preprocessing_test` | 数据关联、时间解析、异常行剔除、CSV 和质量报告生成 | 通过 |

最近一次完整结果：4/4 通过，总耗时约 1.76 秒。原始输出保存在宿主机 `D:\学生资料\.codex-tmp\vm-control\evcs-build-023-report.txt`。

ECharts 第二阶段完成后再次从干净目录构建，4/4 测试通过，总耗时约 1.91 秒；原始输出为 `D:\学生资料\.codex-tmp\vm-control\evcs-build-025-report.txt`。

最终 1.0.0 发布候选从全新目录完成构建、4/4 测试、大屏端到端测试和源码包校验，总测试耗时约 1.84 秒；完整记录为 `D:\学生资料\.codex-tmp\vm-control\evcs-final-035-report.txt`。

1.1.0 发布候选在全新 `/home/bit/codex/evcs-final-131` 目录完成编译，最终回归为 5/5 通过，总测试耗时约 2.85 秒。构建记录为 `D:\学生资料\.codex-tmp\vm-control\evcs-final-build-131.txt`，最终 CTest 记录为 `D:\学生资料\.codex-tmp\vm-control\evcs-final-ctest-135.txt`。

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
- 业务请求由有界线程池处理；每个工作任务使用独立 SQLite 连接，Socket 仅在主线程写回。
- 旧版用户表可自动迁移头像字段并记录 schema version 2。
- 测试曾发现服务端和客户端析构阶段的 Socket 生命周期缺陷，已使用 AddressSanitizer 定位，通过显式解除连接回调修复并加入回归测试。

## 6. 当前非缺陷限制

- 充电硬件、支付和余额均为教学模拟，不连接真实设备或支付渠道。
- 机器学习因无合格真实数据暂缓；接口返回 `FEATURE_NOT_READY`。
- ECharts 6.1.0 已离线集成；统计接口、HTTP 桥接、105 条幂等演示订单和四张图表端到端通过。原始输出为 `D:\学生资料\.codex-tmp\vm-control\evcs-dashboard-026-report.txt`，渲染截图为 `docs/images/dashboard-acceptance.png`。
- 腾讯在线地理编码和路线需要自行配置 Key/应用名；无 Key 时提供北京教学坐标、距离排序和离线导航预览。
- 1.1.0 大屏回归输出 `DASHBOARD_E2E=PASS`，105 条订单全部经预处理导出，质量报告为 105 输入、105 输出、0 拒绝。证据文件分别为 `D:\学生资料\.codex-tmp\vm-control\evcs-final-dashboard-132.txt` 和 `D:\学生资料\.codex-tmp\vm-control\evcs-data-quality-133.json`。
