# 安装、编辑与运行说明

## 1. 验收环境

- Ubuntu 22.04 LTS
- GCC/G++ 11 或更高
- qmake 3.1、Qt 6.2 或更高
- Qt 模块：Core、Concurrent、Network、Sql、Widgets、Test、Charts、WebEngineWidgets
- SQLite Qt 驱动、Python 3.10 或更高

Ubuntu 22.04 安装命令：

```bash
sudo apt update
sudo apt install -y build-essential qmake6 qt6-base-dev libqt6sql6-sqlite \
  libqt6charts6-dev qt6-webengine-dev qt6-webengine-dev-tools python3
```

BitDev 虚拟机已验证 `/usr/bin/qmake6` 使用 Qt 6.2.4。

## 2. 在 Qt Creator 中打开

不要把三个端混在一个工程中。需要编辑哪个程序，就打开对应的唯一工程文件：

- 服务端：`evcs_server.pro`
- 用户端：`evcs_user_client.pro`
- 管理端：`evcs_admin_client.pro`

测试程序也各自提供独立 `.pro`。第一次配置 Kit 时选择系统 Qt 6.2.4/qmake6，构建目录应放在项目目录之外。

## 3. 命令行构建和测试

```bash
cd /path/to/东软电动汽车充电桩应用管理平台
chmod +x ./*.sh
./build_and_test.sh
```

可把自定义构建目录作为第一个参数，例如：

```bash
./build_and_test.sh /tmp/evcs-clean-build
```

构建脚本会依次构建三个应用和四个 C++ 测试，执行界面无显示验收、Python 语法检查与数据预处理测试。

## 4. 启动与数据位置

```bash
./run_server.sh
./run_user_client.sh
./run_admin_client.sh
./run_dashboard.sh
```

运行数据默认写到项目同级的 `evcs-runtime`，不会在工程根目录生成子目录。默认端口：TCP `45454`，大屏 HTTP `8080`。

## 5. 腾讯地图与离线回退

```bash
export EVCS_TENCENT_MAP_KEY='你的-WebService-Key'
export EVCS_TENCENT_MAP_REFERER='你的应用名'
./run_user_client.sh
```

未配置 Key 时，可输入“中关村、海淀、亦庄、大兴、望京、朝阳”等教学位置，程序仍会计算经纬度、距离排序并展示离线导航预览。

## 6. 大屏和数据预处理

```bash
./test_dashboard.sh
python3 preprocess_analytics.py \
  --database ../evcs-runtime/evcharging.db \
  --output-dir /tmp/evcs-processed
```

## 7. 发布包

```bash
./package_release.sh
```

源码压缩包和 SHA-256 文件默认生成在项目同级的 `evcs-release`，压缩包内部仍是完全平铺结构。
