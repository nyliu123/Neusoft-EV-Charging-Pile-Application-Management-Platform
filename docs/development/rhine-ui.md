# 莱茵生命风格 UI

本分支基于主分支 `30fde53`，更新用户端和管理端的 Qt Widgets 表现层。

视觉参考：[Arknights Official — Rhine Lab: Access](https://www.youtube.com/watch?v=-_HeLFGCw7M)。采用研究机构终端的构图：冷白面板、深绿黑导航、黄绿色强调、细网格、编号与仪表刻度。标记和能源示意图为项目自行绘制的矢量图形，不依赖网络图片、游戏立绘或 Qt SVG 插件；项目不是官方莱茵生命软件。

## 界面范围

- 用户端：双栏登录、工作台页眉、找桩页、结构化站点卡片、站点详情、充电流程、个人中心、订单。
- 管理端：双栏登录、编号侧栏、指标与图表、充电桩、站点、用户、订单及弹窗。
- 不修改网络消息、数据库、地图供应商或业务权限。此前主分支的地图、预测模块等集成问题仍须单独处理。
- `web/screen` 在当前主分支只有占位说明，因此本次没有可换肤的 Web 大屏页面。

## 修改入口

`resources/styles/rhine.qss` 是当前两端共用样式；两个 `.qrc` 仍使用 `:/styles/client.qss` 资源别名，因此现有应用入口无需改变加载流程。原 `client.qss` 保留作旧主题参考。

`src/client_ui/rhine_widgets.h` 提供主题标记、登录装饰面板和站点按钮卡片。图形通过 QPainter 绘制，仪表仅为装饰，不伪装成真实电量、连接状态或地图。站点数值仍来自原来的服务端响应，卡片保留 QPushButton 的焦点、键盘和点击行为。

主要色值：背景 `#f0f2eb`、面板 `#fcfdf8`、深色底 `#202e29`、强调 `#c2dd87`、正文 `#202b29`。故障、待结算、充电中继续保留文字状态和不同语义色。

## 构建与验证

```bash
mkdir -p build
cd build
qmake6 ../ev-charging-platform.pro
make -j2
./bin/ev_unit_tests
QT_QPA_PLATFORM=offscreen EV_ORDER_TEST_STYLESHEET=../resources/styles/rhine.qss ./bin/ev_order_ui_tests
bash ../scripts/smoke-test.sh "$PWD"
```

主题界面测试单独构建，使用本机 TCP 测试服务返回明确的模拟数据，不访问真实地图、账号或数据库：

```bash
# 从仓库根目录运行，先完成上面的主工程构建。
mkdir -p build/tests/theme_ui
cd build/tests/theme_ui
qmake6 ../../../tests/theme_ui/theme_ui.pro
make -j2
QT_QPA_PLATFORM=offscreen EV_UI_CAPTURE_DIR="$PWD/captures" ../../bin/ev_theme_ui_tests
```

截图由实际 Qt 控件生成。覆盖用户登录、站点搜索、卡片点击、选桩跳转、预约、开始充电、结算、四步状态指示、个人中心、管理员表单、密码显示切换、后台导航及较小窗口。截图中的站点和经营数字是测试夹具，不是实际运营数据。

## 本次验证记录

2026-09-08，Ubuntu 22.04 / Qt 6.2.4 / GCC 11.3：

- 主工程全量构建通过。
- FoundationTests：32 passed / 0 failed。
- OrderUiTests：3 passed / 0 failed（使用新主题，包含超时与过期响应检查）。
- ThemeUiTests：4 passed / 0 failed（两条完整 UI 场景及初始化、清理）。
- 真实服务端与两个客户端的 TCP 冒烟测试通过。
- 复查默认窗口和较小窗口截图，修正营收图末端数值裁切、站点卡片密度及管理端初始连接状态文字。

无头 Qt 平台产生 `propagateSizeHints` 提示；本次没有 QSS 解析错误。测试覆盖 UI 与现有回归范围，不代表未完成的后台需求已经实现。
