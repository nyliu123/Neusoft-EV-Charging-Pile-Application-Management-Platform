# 东软电动汽车充电桩应用管理平台

面向 Ubuntu 22.04 的 Qt 6/C++ 教学项目，包含充电用户端、运营管理端、中心服务端和 ECharts 数据大屏。当前版本为 **1.2.0**。

## 重构后的工程约定

- 工程目录完全平铺，不创建源代码子目录。
- 三个应用分别使用 `evcs_user_client.pro`、`evcs_admin_client.pro`、`evcs_server.pro`。
- `user_main.cpp`、`admin_main.cpp`、`server_main.cpp` 只负责程序装配。
- 主窗口按“页面创建、用户操作、响应处理、公共装配”拆分，避免在单个 MainWindow 文件堆积代码。
- 所有界面文字使用中文；协议中的英文状态码只在内部传输，界面统一转换为中文。
- 每个页面具有独立 `.qss`，并通过前缀为 `/qss` 的 `.qrc` 加载。
- 构建、运行数据、验收截图和发布包默认放在项目目录的同级目录，不破坏平铺结构。

## 功能范围

- 用户端：手机号快速登录、资料与头像、模拟充值、地图找桩、距离排序、预约、模拟充电、结算和订单。
- 管理端：经营统计、站点—设备联动、设备状态/远程重启、用户与订单筛选、价格和故障管理。
- 服务端：长度前缀 JSON 协议、线程池并发、SQLite 事务、鉴权和结构化日志。
- 大屏与数据：30 天教学数据、ECharts 运营大屏、分析数据清洗和质量报告。
- 暂缓：依赖真实历史数据的负荷预测和智能推荐，不伪造机器学习结果。

## 一键构建与测试

```bash
chmod +x ./*.sh
./build_and_test.sh
```

默认构建到项目同级的 `evcs-qmake-build`，随后执行协议、业务、Socket、界面和预处理测试。

## 启动顺序

在四个终端中依次执行：

```bash
./run_server.sh
./run_user_client.sh
./run_admin_client.sh
./run_dashboard.sh
```

浏览器访问 `http://127.0.0.1:8080`。教学账号：普通用户 `demo / Demo123!`，管理员 `admin / Admin123!`。

详细环境、Qt Creator 打开方法和验收步骤见 `installation.md`、`acceptance-checklist.md` 和 `demo-script.md`。
