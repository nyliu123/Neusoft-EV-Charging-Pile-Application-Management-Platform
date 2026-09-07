# 外部适配器

`MapApiAdapter` 是服务端访问 OpenStreetMap Nominatim 的入口，负责地址解析、内存缓存和每秒最多一次的请求排队。可通过 `EV_NOMINATIM_URL` 配置自建或镜像端点，通过 `EV_MAP_USER_AGENT` 配置带联系信息的 User-Agent。

为保证虚拟机断网演示，大连软件园、高新区、甘井子区和大连北站使用明确标识的离线教学坐标。其他地址仍请求 OpenStreetMap；失败时转换为公共错误码，不伪造距离。

适配器不得直接访问业务表、扣款、修改订单或控制设备，数据库事务内不得等待网络请求。
