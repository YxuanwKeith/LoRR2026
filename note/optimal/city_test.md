### City 单算例测试

- **目标**：固定使用 `city` 的 `paris_1_256_250.json`。
- **口径**：每次只跑 **1 次 `10000 tick`**，再从任务完成 log 统计 `1000 / 5000 / 10000` 的完成任务数。
- **脚本**：`result/run_city_log_benchmark.sh`
- **产物目录**：`result/city_log_benchmark/`

### 固定命令

```bash
bash result/run_city_log_benchmark.sh --label baseline --note "origin baseline"
```

### 结果表

| 时间 | 修改标识 | 1000 tick | 5000 tick | 10000 tick | 备注 |
| --- | --- | ---: | ---: | ---: | --- |
| 2026-03-21 18:56 | `baseline` | 8 | 255 | 346 | origin baseline（沿用上次运行结果） |
| 2026-03-22 02:57 | `scoring_v1_lite` | 12 | 255 | 341 | auto-lite scheduler path |
<!-- CITY_LOG_RESULTS:END -->
