# 数据采集、预处理与分析

数据流水线不修改业务数据库。它用 SQLite 只读连接采集数据，并把输出写到项目同级的 `evcs-runtime/data`，不会在平铺源码目录中生成子目录。

```bash
python3 -m pip install -r analytics-requirements.txt
python3 data_pipeline.py all \
  --database ../evcs-runtime/evcharging.db \
  --data-root ../evcs-runtime/data
```

输出结构：`raw` 保存四张原始 CSV；`processed` 保存清洗后的用户/订单、日/小时/站点日数据、特征数据和 `quality_report.json`；`analysis` 保存站点、用户、时段、营收统计与四张 PNG 图；`models` 和 `predictions` 预留给数据充分后的机器学习阶段。

Ubuntu 22.04 虚拟机也可直接使用系统包：

```bash
sudo apt install -y python3-pandas python3-matplotlib python3-pil
```

Ubuntu 可用 `cron` 在每日 02:00 运行采集，命令为 `0 2 * * * cd /项目目录 && /usr/bin/python3 data_pipeline.py all --database ../evcs-runtime/evcharging.db --data-root ../evcs-runtime/data`。先在终端手动成功执行一次，再加入定时任务。
