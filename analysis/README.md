# 离线训练与预测

本目录实现需求 76–79。流水线只读 SQLite 模拟业务库，按时间顺序构造特征、验证并发布模型，再生成未来 7 个完整自然日和下一完整小时起 24 小时的预测。它不会修改用户、订单、余额或设备状态。

## 一键运行

先启动过服务端，确保数据库已执行迁移并包含已结算订单，然后在仓库根目录执行：

```bash
python3 analysis/run_pipeline.py \
  --database data/ev_charging.sqlite3 \
  --output analysis
```

也可以分步执行：

```bash
python3 analysis/feature_engineering.py --database data/ev_charging.sqlite3 --output analysis
python3 analysis/train_models.py --output analysis
python3 analysis/predict.py --output analysis
```

脚本仅依赖 Python 标准库。模型采用可解释的星期/小时季节均值与短期趋势，和全局历史均值基线比较；候选模型未达到基线容差时不会覆盖已有可用模型。首次运行没有旧模型时会发布候选版本，并在元数据中保留验证结果。

## 产物

- `data/processed/daily_kwh.csv`、`hourly_kwh.csv`：零填充后的日/小时充电量序列。
- `data/processed/train_demand.csv`、`train_peak.csv`：带滞后、滚动、周期特征和时间顺序划分标记的训练集。
- `models/demand_forecast.pkl`、`peak_predict.pkl`：当前启用模型；`models/versions/` 仅保留最近 3 个候选版本。
- `models/model_meta.json`：算法、数据截止时间、候选/基线指标和是否启用。
- `predictions/demand_next7.csv`：7 日预测及 95% 区间。
- `predictions/peak_hours.csv`：24 小时预测、峰/平/谷标记及区间。
- `predictions/idle_predict.csv`：各站点 24 个时间槽预计空闲桩数，始终限制在 0 到设备总数之间。

以上均是教学模拟结果，文件包含 `simulated_data=1` 和模型版本，不应当作真实运营预测。运行产物已由 `.gitignore` 排除，只提交可复现脚本。

## 测试

```bash
python3 -m unittest discover -s analysis/tests -v
```

测试使用临时数据库，检查时间特征输出、模型发布、7/24 时间范围、非负预测和空闲桩边界。
