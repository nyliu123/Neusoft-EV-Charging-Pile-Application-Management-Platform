# 外部适配器

`MapApiAdapter` 是服务端访问腾讯位置服务的唯一入口，提供地址解析、逆地址解析和驾车/步行路线请求。它设置 8 秒超时，将网络、协议与无结果情况统一转换成 `MAP_UNAVAILABLE`，不在数据库事务中等待外部请求。

腾讯地图 Key 只从本地 `config/app.ini` 的 `external/map_api_key` 或环境变量 `TENCENT_MAP_API_KEY` 读取，环境变量优先；Referer 可用 `external/map_referer` 或 `TENCENT_MAP_REFERER` 配置。不要提交真实 Key。

未配置 Key 时，地址中包含“软件园、甘井子、高新园、大连北站、海淀、朝阳、东城”可使用明确标识为“服务端离线教学坐标”的演示位置。其他地址返回地图不可用，用户端仍展示文字站点列表，但不编造距离或路线。
