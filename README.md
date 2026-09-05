# 汽车充电管理平台

本仓库是面向 Ubuntu 22.04 及以上环境的 Qt 6 教学演示项目。系统采用本地客户端/服务端结构：用户端和管理端通过同一 TCP 入口访问领域服务，服务端统一操作 SQLite；Web 大屏和离线分析作为后续模块接入。项目不连接真实充电设备，不执行真实支付。

当前 `main` 主干提供可编译的工程骨架、公共协议、SQLite 初始化、计费基础实现、三个进程入口和单元测试。具体业务规则以 [概要设计说明书](docs/02概要设计说明书第5组（最终版）.docx) 为准。

## 环境要求

- Ubuntu 22.04 或更高版本
- Qt 6.2 或更高版本（Core、Gui、Widgets、Network、Sql、Test）
- qmake6
- 支持 C++17 的编译器

## 构建

```bash
mkdir -p build
cd build
qmake6 ../ev-charging-platform.pro
make -j"$(nproc)"
```

构建结果位于 `build/bin`：

- `ev_server`：业务服务端，默认监听 `127.0.0.1:8888`
- `ev_user_client`：用户端桌面程序
- `ev_admin_client`：管理端桌面程序
- `ev_unit_tests`：基础单元测试

运行测试：

```bash
./build/bin/ev_unit_tests
```

服务端首次启动前，可复制示例配置并按需调整：

```bash
cp config/app.ini.example config/app.ini
./build/bin/ev_server -c config/app.ini
```

未提供配置文件时，服务端使用本机地址和 `data/ev_charging.sqlite3` 等安全演示默认值。密钥和本地配置不得提交。

## 目录

```text
apps/                 用户端、管理端、服务端入口
analysis/             离线统计、预测、推荐与预警任务
config/               qmake 公共配置和运行配置模板
docs/                 设计文档与协作约定
resources/database/   SQLite 迁移脚本
web/screen/            ECharts 只读运营大屏
src/common/           共享类型、常量和统一结果
src/network/          TCP 帧协议与后续网关实现
src/data/             数据库连接与事务基础设施
src/services/         领域服务和无副作用业务计算
src/adapters/         地图和 AI 等外部能力适配层
tests/unit/           Qt Test 单元测试
```

## 协作约定

禁止直接向 `main` 推送。每项工作从最新主干创建短期功能分支，命名为 `feature/<模块>-<说明>`、`fix/<模块>-<说明>` 或 `docs/<说明>`。共享类型、消息编号和接口签名必须先登记到 [共享标识符清单](docs/development/shared-identifiers.md)，再由唯一公共文件定义。

完整提交流程见 [贡献指南](CONTRIBUTING.md)。
