# 总体架构与代码分工

## 1. 运行架构

```text
用户端 ─┐
        ├─ TCP / 长度前缀 JSON ─ 服务端 ─ SQLite（WAL）
管理端 ─┘                         │
                                 ├─ 统计接口 ─ HTTP 桥接 ─ ECharts 大屏
                                 └─ 只读预处理 ─ CSV + 数据质量报告
```

Socket 留在主线程，业务请求进入有界线程池；每个工作任务使用独立 SQLite 连接。服务端是业务规则与状态转换的唯一执行者，客户端不直接访问数据库。

## 2. 平铺文件的命名分工

- `protocol.*`、`domain.*`、`apiclient.*`：共享协议、领域常量和客户端网络访问。
- `chargingserver.*`、`businessservice.*`、`database.*`：服务端连接、业务、持久化。
- `server_settings.*`：配置文件和命令行解析，使 `server_main.cpp` 保持简洁。
- `user_mainwindow.cpp`：用户端窗口装配；`user_pages.cpp`、`user_actions.cpp`、`user_responses.cpp` 分别负责页面、动作、响应。
- `admin_mainwindow.cpp`：管理端窗口装配；`admin_pages.cpp`、`admin_actions.cpp`、`admin_responses.cpp` 分别负责页面、动作、响应。
- `user_*.qss`、`admin_*.qss`：每页独立样式；两个 `.qrc` 均使用 `/qss` 前缀。
- `dashboard_bridge.py`、`dashboard.html`：本地大屏；`preprocess_analytics.py`：只读数据清洗。

所有文件必须直接位于项目根目录。用统一前缀表达模块归属，用独立实现文件代替子目录。

## 3. 关键一致性设计

- 密码只保存随机盐和迭代摘要。
- 请求携带唯一 `requestId`，帧最大 1 MiB。
- 预约、开始充电和停止结算使用 `BEGIN IMMEDIATE` 事务。
- 预约过期会同步释放设备，停止充电会同步结算、扣款、生成订单并释放设备。
- 工作线程只处理业务，通过主线程队列回写 Socket。
- 英文状态码保留在协议和数据库，界面使用 `ui_text.*` 集中翻译为中文。

## 4. 机器学习边界

`analytics.forecast` 与 `recommendation.list` 在没有合格历史数据前返回“功能尚未准备”，机器学习模型不进入核心程序依赖。
