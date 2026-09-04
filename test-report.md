# 1.2.0 需求矩阵整改测试报告

## 1. 测试结论

截至 2026-09-04，1.2.0 需求矩阵符合版已在 BitDev 虚拟机的全新源码目录和全新构建目录中完成 qmake6 编译与回归测试。四组 Qt 自动测试共 17 项全部通过，Python 数据预处理、完整数据流水线和工程结构检查全部通过，编译器 `warning:` 计数为 0。

## 2. 测试环境

- VMware Workstation 17.5.2
- Ubuntu 22.04（BitDev）
- Qt Creator 20.0.1
- qmake 3.1 / Qt 6.2.4
- GCC/G++ 11.3.0
- SQLite Qt 驱动，WAL 模式
- Python 3、pandas 1.3.5、matplotlib 3.5.1、Pillow 9.0.1

最终清洁构建使用：

- 源码目录：`/home/bit/evcs-matrix-v5-final1`
- 构建目录：`/home/bit/evcs-qmake-build-v5-final1`
- 界面截图目录：`/home/bit/evcs-test-artifacts-v5-final1`
- 宿主机构建记录：`D:\学生资料\.codex_work\evcs-matrix-v5-final1-build.log`
- 数据流水线补充复测记录：`D:\学生资料\.codex_work\codex-data-pipeline-final.log`

## 3. 自动化测试

| 测试 | 覆盖内容 | 结果 |
|---|---|---|
| `protocol_test` | 8 字节头、类型/长度校验、JSON 拆包、分段帧和超限帧拒绝 | 4 通过、0 失败 |
| `business_flow_test` | 预约—充电—结算订单闭环、手机号注册、资料与充值、地图回退、筛选、远程重启、统计和旧库迁移 | 6 通过、0 失败 |
| `socket_integration_test` | 双客户端并发预约冲突和 requestId 重复请求幂等 | 4 通过、0 失败 |
| `ui_acceptance_test` | 用户端和管理端真实连接、登录、数据加载、页面渲染与截图 | 3 通过、0 失败 |
| `preprocessing_test.py` | 数据关联、时间解析、异常行、CSV 和质量报告 | 通过 |
| `data_pipeline_test.py` | SQLite 只读采集、清洗、特征、分析、PNG 输出和重复执行 | 通过 |
| `structure_test.py` | 平铺目录、独立 `.pro`、精简入口、中文注释和 QRC/QSS | 通过 |

最终汇总为 17 项 Qt 测试通过、0 失败；Python 两组测试通过；结构检查输出 `STRUCTURE_REQUIREMENTS=PASS`。离屏 UI 环境会输出 Qt 平台插件提示，冲突和未找到场景会输出预期业务告警，均不是编译器告警或测试失败。

## 4. 需求链路验证

用户业务链路已覆盖：

`手机号登录/首次自动注册 → 查询与距离排序 → 站点详情/导航 → 预约并生成订单 → 按订单启动 → 每秒状态推送 → 结束充电 → 余额结算或待结算 → 订单查询`

管理业务链路已覆盖：

`管理员登录 → 经营概览和 7/30 日图表 → 站点/设备联动 → 用户/预约/订单组合筛选 → 价格与故障处理 → 远程重启及日志`

数据链路已覆盖：

`SQLite 只读采集 → 原始 CSV → 清洗数据 → 特征数据 → 质量报告 → 站点/用户/时段/营收分析 → PNG 图表`

## 5. 当前边界

- 充电硬件、支付和余额为教学模拟，不连接真实设备或支付渠道。
- 机器学习需求 65–72 因缺少合格真实历史数据按项目决定暂缓；Socket 和 REST 接口明确返回 `FEATURE_NOT_READY`，不伪造模型和指标。
- 腾讯在线地理编码和路线需要自行配置 Key/应用名；未配置时使用北京教学坐标、距离排序和离线导航预览。
- 需求 1–9、81–89 是团队管理、汇报、录屏和个人文档等过程交付，仍需小组成员分别填写和完成。
- 还需在已登录的 Ubuntu 桌面按 `demo-script.md` 人工点击完整流程并录屏；自动离屏 UI 验收已通过。
