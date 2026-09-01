# 东软电动汽车充电桩应用管理平台

面向 Ubuntu 22.04 的 Qt 6/C++ 教学项目，实现充电用户端、运营管理端和中心服务端。核心业务数据由服务端统一写入 SQLite，两个客户端通过 TCP Socket 与服务端通信。当前版本为 1.1.0。

## 当前范围

- 第一优先级：账号或手机号登录、个人资料/充值、地图找桩、预约、模拟充电、计费结算、订单查询。
- 第二优先级：站点—设备联动、充电桩、用户、订单、价格、故障、远程重启和 Qt Charts 经营统计管理。
- 第三优先级：ECharts 运营数据大屏。
- 数据准备：提供可重复的数据清洗/导出流水线和质量报告。
- 暂缓：依赖真实历史数据的机器学习模型；当前只保留接口和数据字段，不伪造训练结果。

## 构建

```bash
chmod +x scripts/*.sh
./scripts/build_and_test.sh
```

生成的程序位于 `build/bin`：

- `evcs_server`：中心服务端；
- `evcs_user_client`：充电用户端；
- `evcs_admin_client`：运营管理端。

## 文档

- `docs/requirements-baseline.md`：已冻结的第一阶段需求与验收边界；
- `docs/architecture.md`：总体架构、模块和数据流；
- `docs/socket-protocol.md`：TCP/JSON 通信协议；
- `database/schema.sql`：SQLite 数据库结构。

## 运行演示

从项目根目录打开三个终端：

```bash
./scripts/run_server.sh
./scripts/run_user_client.sh
./scripts/run_admin_client.sh
./scripts/run_dashboard.sh
```

大屏地址为 `http://127.0.0.1:8080`，可点击“生成 30 天演示数据”。页面使用随项目离线交付的 Apache ECharts 6.1.0，不依赖演示现场网络，并明确标注数据为教学模拟。

演示账号仅用于本地教学环境：

- 普通用户：`demo` / `Demo123!`
- 管理员：`admin` / `Admin123!`

服务端默认读取 `config/server.json`，数据保存到 `data/evcharging.db`，结构化日志写入 `logs/evcs-server.jsonl`。命令行的 `--listen`、`--port`、`--database`、`--schema` 和 `--log` 可覆盖配置文件。

无腾讯地图 Key 时仍可用内置北京教学坐标完成距离排序和离线导航预览；在线地址解析与路线展示的配置方法见 `docs/installation.md`。
