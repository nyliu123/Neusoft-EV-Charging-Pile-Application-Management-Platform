# ECharts 运营数据大屏

大屏通过本地 Python 标准库桥接服务访问 Qt TCP 服务端，不直接读取 SQLite。页面依赖已离线保存的 Apache ECharts 6.1.0，因此演示时不需要外网。

## 启动

先启动 `evcs_server`，再执行：

```bash
./scripts/run_dashboard.sh
```

浏览器访问 `http://127.0.0.1:8080`。桥接服务默认使用本地教学管理员账号；可通过环境变量覆盖：

```bash
EVCS_ADMIN_USERNAME=admin EVCS_ADMIN_PASSWORD='your-password' ./scripts/run_dashboard.sh
```

页面上的“生成 30 天演示数据”会写入带 `DEMO-` 前缀的可重复订单；相同日期再次执行不会重复写入。所有图表均明确标注为教学演示数据。

## 第三方组件

- Apache ECharts 6.1.0
- 文件：`vendor/echarts.min.js`
- 许可证：`vendor/ECHARTS-LICENSE.txt`
- SHA-256：`B66B25AEB4DF84E33199DC21694014D336D222CBD9DEB0E5A7C14BD6AA0D0FD0`
