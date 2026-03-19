### 本地回归测试整理

- 生成日期: 2026-03-19
- 运行方式: `result/run_local_regression.sh`
- 仿真参数: `-s 1000 -c 1 --prettyPrintJson`
- CTest 日志: `result/ctest.log`
- 补充实验: 2026-03-20 已新增 `LORR_ENABLE_TASK_SCORING=1` 的 `city / game 10000 tick` 长时结果

| 测试集 | 样例 | teamSize | numTaskFinished | makespan | numPlannerErrors | numScheduleErrors | numEntryTimeouts | 可视化文件 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| city | `paris_1_256_250.json` | 250 | 26 | 1000 | 0 | 0 | 0 | `result/city_paris_1_256_250.viz.json` |
| game | `brc202d_500.json` | 500 | 28 | 1000 | 0 | 0 | 0 | `result/game_brc202d_500.viz.json` |
| random | `random_32_32_20_100.json` | 100 | 98 | 1000 | 0 | 0 | 0 | `result/random_32_32_20_100.viz.json` |
| warehouse | `warehouse_small_200.json` | 200 | 453 | 1000 | 0 | 0 | 0 | `result/warehouse_small_200.viz.json` |

### PlanViz 启动方案

当前仓库已存在 `PlanViz/.venv` 环境，可直接使用。若需重建，再执行:

```bash
python3.11 -m venv .venv-planviz
source .venv-planviz/bin/activate
pip install -r PlanViz/requirements.txt
```

PlanViz 是单窗口交互工具，每次只打开一个场景。按需执行其中一条命令:

```bash
# city
PlanViz/.venv/bin/python PlanViz/script/run.py --map example_problems/city.domain/maps/Paris_1_256.map --plan result/city_paris_1_256_250.viz.json
# game
PlanViz/.venv/bin/python PlanViz/script/run.py --map example_problems/game.domain/maps/brc202d.map --plan result/game_brc202d_500.viz.json
# random
PlanViz/.venv/bin/python PlanViz/script/run.py --map example_problems/random.domain/maps/random-32-32-20.map --plan result/random_32_32_20_100.viz.json
# warehouse
PlanViz/.venv/bin/python PlanViz/script/run.py --map example_problems/warehouse.domain/maps/warehouse_small.map --plan result/warehouse_small_200.viz.json
```

也可以直接使用 `result/open_planviz.sh <city|game|random|warehouse>`。

### 任务评分长时测试补充

- 开关: `LORR_ENABLE_TASK_SCORING=1`
- 统计参数: `-s 10000 -c 3 -d 3 --prettyPrintJson`
- 说明: 下表仅补充本轮已实际执行的 `city / game` 两个场景，便于和基线 `10000 tick` 结果直接比较。

| 测试集 | 样例 | teamSize | numTaskFinished | makespan | 对比基线 | summary 文件 |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| city | `paris_1_256_250.json` | 250 | 375 | 10000 | `381 -> 375` | `result/city_paris_1_256_250.taskscore.10000.summary.json` |
| game | `brc202d_500.json` | 500 | 500 | 10000 | `491 -> 500` | `result/game_brc202d_500.taskscore.10000.summary.json` |
