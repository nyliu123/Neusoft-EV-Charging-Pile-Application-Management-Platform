# 汽车充电管理平台

本仓库是面向 Ubuntu 22.04 及以上环境的 Qt 6 教学演示项目。系统采用本地客户端/服务端结构：用户端和管理端通过同一 TCP 入口访问领域服务，服务端统一操作 SQLite；Web 大屏和离线分析作为后续模块接入。项目不连接真实充电设备，不执行真实支付。

当前 `main` 主干提供可编译的工程骨架、公共协议、SQLite 初始化、计费基础实现、三个进程入口和单元测试，并已实现登录注册与用户信息模块的 UML-010～UML-017。具体业务规则以 [概要设计说明书](docs/02概要设计说明书第5组（最终版）.docx) 为准。

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

### Qt Creator 打开方式

在 Qt Creator 中选择“文件 → 打开文件或项目”，打开仓库根目录下的 `ev-charging-platform.pro`，不要单独打开 `apps` 或 `src` 中的子项目。选择 Desktop Qt 6 Kit，并使用 Qt Creator 建议的影子构建目录。

顶层工程使用标准 qmake `SUBDIRS` 结构，项目树中应显示 `common`、`network`、`data`、`services`、`server`、`user_client`、`admin_client` 和 `unit_tests` 八个子项目。若本机曾打开过旧版本，先关闭项目并删除本机生成的 `ev-charging-platform.pro.user*`，然后重新打开顶层工程，以免 Qt Creator 继续使用旧解析缓存。

构建结果位于 `build/bin`：

- `ev_server`：业务服务端，默认监听 `127.0.0.1:8888`
- `ev_user_client`：用户端桌面程序
- `ev_admin_client`：管理端桌面程序
- `ev_unit_tests`：基础单元测试

运行测试：

```bash
./build/bin/ev_unit_tests
./scripts/smoke-test.sh
```

冒烟测试会短暂启动服务端，并让用户端、管理端以无界面检查模式完成真实 TCP 协议握手。运行数据和日志写入 `build/smoke`。

服务端首次启动前，可复制示例配置并按需调整：

```bash
cp config/app.ini.example config/app.ini
./build/bin/ev_server -c config/app.ini
```

未提供配置文件时，服务端使用本机地址和 `data/ev_charging.sqlite3` 等安全演示默认值。密钥和本地配置不得提交。

## 最小运行

在三个终端分别执行：

```bash
./build/bin/ev_server
./build/bin/ev_user_client
./build/bin/ev_admin_client
```

两个客户端会自动连接 `127.0.0.1:8888`，界面显示“服务可用，协议握手成功”。用户端可对状态为 `normal` 的已有用户执行手机号免密登录；冻结账号会被拦截，未注册手机号会自动创建默认用户并直接进入首页。登录后客户端每30秒续期内存会话，主动退出、会话失效或连接断开时清除登录态并返回登录页。
登录后的个人信息页支持服务端刷新、修改昵称、上传头像和模拟钱包充值，操作成功后会同步本地会话缓存。

不同成员可使用独立端口和数据库，避免本机调试互相影响：

```bash
./build/bin/ev_server --port 18881 --database build/dev/member-a.sqlite3
./build/bin/ev_user_client --port 18881
```

## 分支独立调试

首次进入新工作树时先执行一次顶层 `qmake6`。之后可只构建自己的目标及其声明依赖：

```bash
cd build
qmake6 ../ev-charging-platform.pro
make user_client      # 用户端及依赖
make admin_client     # 管理端及依赖
make server           # 服务端及依赖
make network          # 公共网络库
make services         # 领域服务库
make unit_tests       # 单元测试
```

建议每名成员使用自己的 Git 功能分支；如果需要同时保留多个分支，使用 `git worktree` 创建独立工作目录。各工作树使用自己的 `build` 目录、调试端口和 SQLite 文件。功能调好后推送对应分支交由组长审核，未完成分支不会影响 `main` 或其他工作树。

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
