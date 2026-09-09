# Apple 风格轻充 UI

> 本文记录前一版 Apple 界面（`fb9aebf`）。本分支后续的现代化版本已改用顶部导航和 `modern.qss`，当前版本请参见 [现代 UI 说明](modern-ui.md)。

`feature/apple-ui` 基于主分支 `1d1a1c0`，沿用莱茵主题中已验证的界面改进，再重做 Apple / macOS 风格的表现层。`feature/rhine-lab-ui` 独立保留，不覆盖、不强推主分支。

## 设计依据与取舍

参考用户提供的 [emilkowalski/skills](https://github.com/emilkowalski/skills)，读取版本 `d23d7f88a2e21c9e4b1418c7abe420f5c1052ba7` 中的 `apple-design` 与 `emil-design-eng`。这些设计原则按 Qt Widgets 的能力适配，并未安装或运行该仓库的脚本。

- 内容优先：浅灰背景 `#f5f5f7`、白色表面、深色正文 `#1d1d1f`，蓝色用于主要操作和当前导航。金额、空闲数量和距离保持真实服务端字段，不用装饰性数值填充界面。
- 清晰分组：两端统一持久侧栏，登录与管理表单分组，站点卡片分开显示名称、地址、价格、空闲数、评分和距离。长地址/热评截短显示，完整文字保留在辅助功能名称及提示中。
- 即时反馈：按钮提供按下、悬停、禁用和键盘焦点状态。导航、搜索、表单下拉属于高频操作，直接更新，不加装饰动画或人为延时。当前主题无位移动画，也无需依赖减少动态效果设置。
- 克制材质：使用圆角、轻量边框和地图浮层的浅阴影。Qt Widgets 主界面采用不透明表面，避免假毛玻璃降低文字对比度；不叠加实时模糊层。
- 熟悉且可用：保留原生窗口控制和 Qt 键盘行为，不制作不可点击的假 macOS 红黄绿按钮。使用系统已安装字体及 Qt 高 DPI 缩放，不捆绑 Apple 专有字体、图标或商标素材；“轻充”图标和插画是 QPainter 原创矢量图。

这不是 Apple 官方应用，也不是 SwiftUI / Liquid Glass 原生实现；目标是跨平台的简洁桌面体验。

## 代码入口与兼容性

- `resources/styles/apple.qss`：用户端与管理端统一样式，两个 `.qrc` 仍通过 `:/styles/client.qss` 别名加载。
- `src/client_ui/apple_widgets.h`：品牌、矢量符号、登录面板、站点卡片。卡片仍是 QPushButton，保留点击、焦点、文本与辅助功能名称。
- `apps/user_client/user_home_widget.cpp`：侧栏 `userNavigation` 与原 QTabWidget 双向同步。原四个页签的文字、索引、业务连接不变；程序跳转、鼠标和键盘都使用同一选中状态。
- `apps/admin_client/admin_main_window.cpp`：五项运营导航、标题和会话状态；指标和图表在原查询上展示。
- `apps/user_client/navigation_map_dialog.cpp`：仅调整地图配色、浮层与按钮反馈，保留 OpenStreetMap / Valhalla 署名及路线逻辑。

保留主分支的站点评分、查看/发布/修改评论、点赞、内嵌地图，以及登录、找桩、预约、充电、结算、钱包、订单和后台管理流程。没有修改 TCP 协议、业务服务、数据库或支付规则。`web/screen` 仍是占位模块，不在本次桌面换肤范围内。

## 构建与自动测试

环境：Ubuntu 22.04，Qt 6.2.4（含 WebEngineWidgets），GCC 11.3，qmake6；无头地图测试另需 `xvfb-run`。先按 README 安装项目依赖。

```bash
mkdir -p build
cd build
qmake6 ../ev-charging-platform.pro
make -j2
./bin/ev_unit_tests
QTWEBENGINE_CHROMIUM_FLAGS="--disable-gpu" \
  EV_ORDER_TEST_STYLESHEET=../resources/styles/apple.qss \
  xvfb-run -a ./bin/ev_order_ui_tests
bash ../scripts/smoke-test.sh "$PWD"
mkdir -p tests/theme_ui
cd tests/theme_ui
qmake6 ../../../tests/theme_ui/theme_ui.pro
make -j2
QT_QPA_PLATFORM=offscreen EV_UI_CAPTURE_DIR="$PWD/captures" ../../bin/ev_theme_ui_tests
```

`ThemeUiTests` 使用本机 TCP 夹具，覆盖登录、找桩卡片、评分/评论/点赞、选择充电桩、预约、充电、结算、侧栏双向同步、键盘导航、个人中心、管理员密码显示与五项后台导航。`OrderUiTests` 检查订单超时/迟到响应及地图加载、出行方式、拖拽、缩放、失败和恢复；地图使用本地 HTTP 夹具。

截图来自实际 Qt 控件，不是网页样机；其中的账号、站点和经营数字都是测试数据。公共路线服务的真实联网用例默认跳过（可设置 `EV_MAP_REAL_ROUTE_TEST=1` 单独运行），离线测试不证明公网服务可用。

## 验证记录

2026-09-08，Ubuntu 22.04 / Qt 6.2.4 / GCC 11.3：

- 主工程和主题测试工程编译通过。
- FoundationTests：32 passed，0 failed。
- ThemeUiTests：4 passed，0 failed；包含评论、点赞、预约到结算及鼠标/键盘导航。
- OrderUiTests：4 passed，0 failed，1 skipped；跳过默认禁用的真实公网路线用例。
- 独立测试数据库上的服务端、用户端、管理端 TCP 握手冒烟测试通过。
- 复查实际登录、找桩、评论、地图、充电、个人中心、订单和管理页面截图，以及用户端 900×600、管理端 1100×680 窗口。截图中的地图瓦片为测试占位，不是公网地图截图。
- 为虚拟机补齐了 README 已要求的 `libqt6webenginecore6-bin` 运行包；没有改变地图服务商或降低业务权限。

无头 Qt 的 `propagateSizeHints` 提示不影响用例；未发现主题 QSS 解析错误。上述验证只覆盖现有自动测试与已复查界面，不代表项目的全部后台需求已实现。
