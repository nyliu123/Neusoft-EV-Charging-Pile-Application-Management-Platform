# 安装与运行说明

## 1. 环境要求

- Ubuntu 22.04 LTS
- GCC/G++ 11 或更高
- CMake 3.22 或更高
- Qt 6.2 或更高：Core、Network、Sql、Widgets、Test
- Qt SQLite 驱动
- Python 3.10 或更高（仅数据大屏桥接使用）
- 任意现代浏览器（数据大屏）

Ubuntu 22.04 可安装：

```bash
sudo apt update
sudo apt install -y build-essential cmake qt6-base-dev libqt6sql6-sqlite python3
```

Qt Creator 只用于图形化开发，不是命令行构建的必需项。

## 2. 构建和测试

```bash
cd /path/to/东软电动汽车充电桩应用管理平台
chmod +x scripts/*.sh tools/dashboard_bridge.py
./scripts/build_and_test.sh
```

构建成功后，`build/bin` 包含服务端、用户端、管理端和测试程序。

## 3. 启动顺序

分别打开四个终端：

```bash
./scripts/run_server.sh
./scripts/run_user_client.sh
./scripts/run_admin_client.sh
./scripts/run_dashboard.sh
```

浏览器打开 `http://127.0.0.1:8080`。第一次展示大屏时可点击“生成 30 天演示数据”。

## 4. 默认本地配置

- TCP：`0.0.0.0:45454`
- 大屏 HTTP：`127.0.0.1:8080`
- SQLite：`data/evcharging.db`
- 日志：`logs/evcs-server.jsonl`
- 用户端教学账号：`demo` / `Demo123!`
- 管理端教学账号：`admin` / `Admin123!`

这些账号只用于隔离的课堂演示。若服务端暴露给其他机器，应先修改种子账号策略和密码，并限制监听地址、防火墙和日志访问权限。

## 5. 可重复验收

```bash
./scripts/build_and_test.sh
./scripts/test_dashboard.sh
```

前一命令应显示 4/4 CTest 通过；后一命令应输出 `DASHBOARD_E2E=PASS`。

## 6. 打包

```bash
./scripts/package_release.sh
```

脚本在 `release` 目录生成源码交付包和 SHA-256 校验文件，不包含 Git、构建目录、运行数据库和日志。
