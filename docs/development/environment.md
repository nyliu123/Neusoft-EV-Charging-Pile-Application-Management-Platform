# Ubuntu 与 Qt 开发环境

目标运行环境为 VMware 17 Pro 中的 Ubuntu 22.04 或更高版本；桌面客户端和服务端使用 Qt 6.2 或更高版本、C++17、qmake 与 SQLite 驱动。每次交付应在录屏或测试记录中写明实际 Ubuntu、Qt 和编译器版本。

## 安装与版本核对

Ubuntu 22.04 可先安装下列基础包：

```bash
sudo apt update
sudo apt install build-essential qt6-base-dev qt6-base-dev-tools qmake6 libqt6sql6-sqlite python3
```

核对实际版本：

```bash
lsb_release -ds
qmake6 -v
g++ --version
python3 --version
```

Qt Creator 可使用系统软件源或 Qt 官方安装器安装。打开仓库根目录的 `ev-charging-platform.pro`，选择 Desktop Qt 6 Kit；不要只打开某个子项目。

## 构建、测试与运行

```bash
mkdir -p build
cd build
qmake6 ../ev-charging-platform.pro
make -j"$(nproc)"
./bin/ev_unit_tests
cd ..
./scripts/smoke-test.sh
```

在三个终端分别运行 `build/bin/ev_server`、`build/bin/ev_user_client`、`build/bin/ev_admin_client`。若需要腾讯地图真实地址解析，在启动服务端前设置本机环境变量：

```bash
export TENCENT_MAP_API_KEY='你的本地Key'
export TENCENT_MAP_REFERER='已在腾讯位置服务登记的Referer'
```

不配置 Key 时仍可用预设教学坐标验证找桩、距离排序和详情查询；地图不可用时必须保留文字站点列表。

## 可复现检查记录

提交前记录：操作系统版本、Qt/qmake 版本、编译器版本、构建命令、单元测试结果、冒烟测试结果、使用的端口及数据库是否由全新文件迁移生成。不得记录地图 Key、管理员密码哈希或个人数据库。
