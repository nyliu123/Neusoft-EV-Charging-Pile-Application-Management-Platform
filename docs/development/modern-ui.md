# 轻充 · 现代界面

2026-09-09，在 Apple 版本 `fb9aebf` 上调整表现层，沿用 `feature/apple-ui` 分支。前一版完整界面仍可通过提交 `fb9aebf` 查看；本次不修改 `main` 或 `feature/rhine-lab-ui`。

## 设计调整

- 用户端只有四个一级入口，改为顶部胶囊导航，给主要内容留出横向空间；导航仍与原 QTabWidget 双向同步，支持程序跳转、鼠标与左右方向键。
- 找桩页使用深靛蓝主题横幅、独立搜索区与站点网格。价格、距离和空闲数分别呈现，不依赖颜色区分状态。卡片整体仍是 QPushButton，保留空格激活、焦点与辅助功能名称。
- 网格按自身宽度切换三列（至少 1050）、两列（至少 680）或一列；列数改变时移动原控件，不重建业务连接或丢失键盘焦点。结果区域单独滚动，页面高度不足 590 时横幅由 172 缩为 88，优先让第一行站点信息完整可见。
- 管理端保留侧栏，便于反复切换运营表格；侧栏与内容分开，主要指标使用深色卡片，图表采用低饱和配色。
- 统一留白、圆角、文字层次和悬停/按下/禁用/焦点状态。充电插画由 QPainter 原创绘制，按控件尺寸缩放；它是装饰，不表示设备实时状态。未引入远程图片、专有字体、实时模糊或装饰性动画。

## 代码入口

- `resources/styles/modern.qss`：两个客户端通过原 `:/styles/client.qss` 别名加载。旧 `apple.qss` 与莱茵主题文件仍保留；仅切换 QSS 并不能恢复旧版布局，完整旧版以 Git 提交为准。
- `src/client_ui/apple_widgets.h`：为兼容现有引用，保留原文件及公共类名；包含品牌、主题插画、横幅、站点按钮和 AdaptiveStationGrid。
- `apps/user_client/user_home_widget.cpp`、`station_search_widget.cpp`：用户导航、搜索页布局。原业务请求、字段、评论与地图入口保持不变。
- `apps/admin_client/admin_main_window.cpp`、`admin_charts.cpp`、`admin_dashboard_page.cpp`：管理端布局与图表表现。
- `tests/theme_ui/tst_theme_ui.cpp`：完整用户路径、后台导航及响应式网格测试。

没有改动协议、业务服务、数据库、计费或支付规则。截图取自实际 Qt 控件，账号、站点和经营数据来自本机测试夹具；不是线上运营数据。

## 构建与验证

沿用 README 的 Ubuntu 22.04 / Qt 6.2+ 构建方法。完整构建后运行：

```bash
./build/bin/ev_unit_tests
QTWEBENGINE_CHROMIUM_FLAGS="--disable-gpu" \
  EV_ORDER_TEST_STYLESHEET="$PWD/resources/styles/modern.qss" \
  xvfb-run -a ./build/bin/ev_order_ui_tests
bash scripts/smoke-test.sh "$PWD/build"
mkdir -p build/tests/theme_ui
cd build/tests/theme_ui
qmake6 ../../../tests/theme_ui/theme_ui.pro
make -j2
QT_QPA_PLATFORM=offscreen EV_UI_CAPTURE_DIR="$PWD/captures" ../../bin/ev_theme_ui_tests
```

主题测试覆盖用户登录、站点卡片、评论/点赞、预约、充电、结算、个人中心、订单导航和后台五项导航；新增网格 3→2→1→3 列重排、焦点保留、空格点击与清空测试，并检查用户端 900×600 窗口。

地图测试使用本地 HTTP 夹具。真实公网路线用例默认跳过，离线测试不证明公网服务可用；地图截图中的瓦片是测试占位。

### 本次验证记录

2026-09-09，Ubuntu 22.04 / Qt 6.2.4 / GCC 11.3：

- 主工程、订单/地图测试和主题测试编译通过。
- FoundationTests：32 passed，0 failed。
- ThemeUiTests：5 passed，0 failed。
- OrderUiTests：4 passed，0 failed，1 skipped（默认禁用的真实公网路线用例）。
- 独立测试数据库上的服务端、用户端、管理端 TCP 握手冒烟测试通过。
- 复查了实际登录、找桩、评论、站点详情、充电、个人中心、订单、地图和后台截图；用户端 900×600 下完整显示第一行站点卡片，管理端 1100×680 下图表与指标无重叠。
- `git diff --check` 通过，未发现 QSS 解析错误。上述结果不代表所有后台需求均已完成，也不覆盖所有系统与 DPI 设置。
