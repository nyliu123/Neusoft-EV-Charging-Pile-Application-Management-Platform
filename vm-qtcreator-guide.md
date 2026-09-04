# 在 Ubuntu 虚拟机的 Qt Creator 中打开和运行 App

本文适用于课程提供的 BitDev 虚拟机。已验证环境为 Ubuntu 22.04、Qt Creator 20.0.1、系统 qmake6/Qt 6.2.4。

## 1. 把压缩包放进虚拟机

1. 在 Windows 中找到项目源码压缩包。
2. 启动 VMware，登录 Ubuntu，账号 `bit`，密码 `123456`。
3. 最简单的传输方式是把压缩包从 Windows 文件管理器拖到 Ubuntu 桌面；如果拖放不可用，使用 VMware 的“虚拟机 → 设置 → 选项 → 共享文件夹”，添加压缩包所在目录。
4. 在 Ubuntu 文件管理器中把压缩包复制到“主目录”，右键“提取到此处”。建议最终目录为 `/home/bit/evcs-app-1.2`。
5. 目录中应直接看到 `evcs_server.pro`、`evcs_user_client.pro`、`evcs_admin_client.pro`，不应再套一层同名目录。

也可以在终端解压：

```bash
mkdir -p /home/bit/evcs-app-1.2
unzip ~/项目源码压缩包.zip -d /home/bit/evcs-app-1.2
cd /home/bit/evcs-app-1.2
chmod +x ./*.sh
```

## 2. 第一次先做完整环境验收

打开终端执行：

```bash
cd /home/bit/evcs-app-1.2
./build_and_test.sh /home/bit/evcs-build-1.2
```

最后出现“构建与测试完成”才算通过。若提示缺少 pandas 或 matplotlib，执行：

```bash
sudo apt update
sudo apt install -y python3-pandas python3-matplotlib python3-pil
```

## 3. 配置 Qt Creator 的 Qt 6 Kit

1. 打开 Qt Creator。
2. 进入“编辑/Edit → Preferences/首选项 → Kits”。
3. 在“Qt Versions”中确认存在 `/usr/bin/qmake6`，版本应显示 Qt 6.2.4；若没有，点击“Add”手动选择 `/usr/bin/qmake6`。
4. 在“Kits”中选择或新建 Desktop Kit，编译器选择系统 GCC/G++，Qt version 选择刚才的 Qt 6.2.4。
5. 构建目录选项目目录之外，例如 `/home/bit/qt-build/server`，避免把构建产物混进源码。

## 4. 打开并运行服务端

1. 选择“文件/File → 打开文件或项目/Open File or Project”。
2. 选择 `/home/bit/evcs-app-1.2/evcs_server.pro`。
3. 在配置页勾选 Qt 6.2.4 Desktop Kit，点击“Configure Project”。
4. 点击左下角锤子图标构建；构建成功后点击绿色三角运行。
5. 服务端窗口/日志显示监听 `127.0.0.1:8888` 后保持它运行。

## 5. 同时打开用户端和管理端

服务端不能关闭。最稳妥的做法是再开两个 Qt Creator 窗口：

```bash
qtcreator /home/bit/evcs-app-1.2/evcs_user_client.pro &
qtcreator /home/bit/evcs-app-1.2/evcs_admin_client.pro &
```

分别选择同一个 Qt 6.2.4 Desktop Kit，构建目录可用：

- 用户端：`/home/bit/qt-build/user`
- 管理端：`/home/bit/qt-build/admin`

先运行用户端，再运行管理端：

- 用户端：使用手机号 `13800138000` 登录；输入一个新的合法 11 位手机号会自动注册，初始余额 0 元。
- 管理端：用户名 `admin`，密码 `123456`。密码框不会预填，这是安全要求。

## 6. 运行数据大屏

在终端执行：

```bash
cd /home/bit/evcs-app-1.2
./run_dashboard.sh
```

浏览器访问 `http://127.0.0.1:8080`。机器学习预测、推荐和预警接口在数据不足时会明确返回 `FEATURE_NOT_READY`，不会伪造结果。

## 7. 常见问题

- 客户端一直显示未连接：先确认服务端正在运行，并且三端端口都是 `8888`。
- 找不到 Qt Kit：在 Qt Versions 中添加 `/usr/bin/qmake6`，不要选择 Qt 5 的 qmake。
- Qt WebEngine 无法显示地图：先确认安装 `qt6-webengine-dev`；没有腾讯地图 Key 时仍可使用预设位置和离线预览。
- 旧数据库导致账号或结构异常：先备份项目同级 `evcs-runtime`；测试时可改用新的运行目录，不要直接删除唯一数据副本。
- 修改源码后运行的仍是旧程序：执行“构建/Build → Run qmake”，然后“Rebuild Project”。

## 8. 正确关闭

先在 Qt Creator 中停止用户端、管理端和服务端，再关闭 Qt Creator。最后通过 Ubuntu 右上角菜单正常关机，不要直接关闭 VMware 电源，以免数据库或源码损坏。
