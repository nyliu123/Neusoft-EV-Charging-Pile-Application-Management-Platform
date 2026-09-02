# 1.2.0 重构测试报告

## 1. 测试结论

截至 2026-09-02，1.2.0 平铺 qmake 重构版已在 BitDev 虚拟机内使用全新源码目录和全新构建目录完成编译与回归测试。四组 Qt 自动测试、数据预处理测试、结构规则检查和浏览器大屏端到端测试全部通过，编译器警告为 0。

## 2. 测试环境

- VMware Workstation 17.5.2
- Ubuntu 22.04（BitDev）
- qmake 3.1
- Qt 6.2.4
- GCC/G++ 11.3.0
- SQLite Qt 驱动，WAL 模式

最终清洁构建使用：

- 源码目录：`/home/bit/codex/evcs-flat-final-124-src`
- 构建目录：`/home/bit/codex/evcs-qmake-build-final-124`
- 界面截图目录：`/home/bit/codex/evcs-test-artifacts-final-124`
- 宿主机构建记录：`D:\学生资料\.codex-tmp\vm-control\evcs-flat-build-final-124.log`

## 3. 自动化测试

| 测试 | 覆盖内容 | 结果 |
|---|---|---|
| `protocol_test` | 长度前缀 JSON 拆包、分段帧和超限帧拒绝 | 4 通过、0 失败 |
| `business_flow_test` | 充电主流程、手机号注册、资料与充值、地图回退、筛选、远程重启、统计和旧库迁移 | 6 通过、0 失败 |
| `socket_integration_test` | 两个真实 TCP 客户端并发预约同一充电桩，仅一个成功 | 3 通过、0 失败 |
| `ui_acceptance_test` | 用户端和管理端真实连接、登录、数据加载、页面渲染与截图 | 3 通过、0 失败 |
| `preprocessing_test.py` | 数据关联、时间解析、异常行处理、CSV 和质量报告生成 | 通过 |
| `structure_test.py` | 根目录无子目录、独立 `.pro`、精简入口、中文注释、QRC/QSS 页面对应关系 | 通过 |

编译日志中 `warning:` 计数为 0，结构检查输出 `STRUCTURE_REQUIREMENTS=PASS`。

## 4. 端到端验证

浏览器大屏已连接真实服务端执行端到端测试，输出：

`DASHBOARD_E2E=PASS {"chargerCount": 6, "energyWh": 1376110, "orderCount": 105, "revenueCents": 139946}`

原始记录：`D:\学生资料\.codex-tmp\vm-control\evcs-flat-dashboard-final-124.log`。

用户业务链路已覆盖：

`手机号登录/自动注册 → 查询站点 → 预约或直接充电 → 查询实时状态 → 结束充电并结算 → 查询订单`

管理业务链路已覆盖：

`管理员登录 → 经营概览 → 站点/充电桩/用户/预约/订单查询 → 价格与故障处理 → 远程重启及日志`

## 5. 重构规则验证

- 用户可见界面采用中文，状态与角色名称统一转换。
- 关键业务、网络、数据库和安全代码包含中文注释。
- 三个入口文件均不超过 40 行，只负责程序装配。
- 用户端和管理端 MainWindow 已拆分为页面、动作、响应和样式文件。
- 项目根目录完全平铺，不包含代码子目录。
- 服务端、用户端、管理端各有唯一应用 `.pro` 文件。
- 两个 `.qrc` 均使用 `/qss` 前缀，且只包含 `.qss`。
- 每个业务页面都绑定独立 `.qss` 文件。

## 6. 当前非缺陷限制

- 充电硬件、支付和余额为教学模拟，不连接真实设备或支付渠道。
- 机器学习因缺少合格真实数据暂缓，接口明确返回功能未就绪。
- 腾讯在线地理编码和路线需要自行配置 Key/应用名；未配置时使用北京教学坐标、距离排序和离线导航预览。
- 尚需小组在虚拟机桌面按 `demo-script.md` 人工点击并录屏，并填写组号、组长、成员和签字信息。
