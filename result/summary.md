### 迭代记录文档

用于持续记录每次迭代的实现内容、测试情况与核心衡量指标，主表统一按“版本 + makespan”组织，便于直接比较同一算法在 `1000` 和 `10000` 两个 horizon 下的表现。

### 版本简表

| 迭代 | 日期 | 版本定位 | 任务分配 | 路径规划 | 执行决策 | 构建/测试 | 备注 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 迭代 01 | 2026-03-19 | 初版可运行系统 | Hungarian 二分图匹配 | PIBT | TPG | 4 / 4 CTest 通过 + 4 域样例回归 | 基线版本 |
| 迭代 02 | 2026-03-19 | 本地回归与可视化整理 | Hungarian 二分图匹配 | PIBT | TPG | 5 / 5 CTest 通过 + 4 域可视化输出 | 固化 `result/*.viz.json` 与 PlanViz 打开方式 |
| 迭代 03 | 2026-03-19 | `task_assign` 迁移后统计口径 | Hungarian 二分图匹配 | PIBT + flow guide path | TPG | 5 / 5 CTest 通过 + stats-only 回归 | 默认不生成 `viz.json`，仅在指定时生成 |
| 迭代 04 | 2026-03-20 | 任务评分函数（静态 + 动态拥堵） | Hungarian + task scoring | PIBT + flow guide path | TPG | `city / game` `10000 tick` 长时回归 | 评分默认关闭，开启方式为 `LORR_ENABLE_TASK_SCORING=1` |

### 主统计表

| 版本 | makespan | 任务分配 | 路径规划 | 执行决策 | city | game | random | warehouse | 备注 |
| --- | ---: | --- | --- | --- | ---: | ---: | ---: | ---: | --- |
| 迭代 03 | 1000 | Hungarian | PIBT + flow guide path | TPG | 27 | 29 | 96 | 463 | `-c 3 --prettyPrintJson` |
| 迭代 03 | 10000 | Hungarian | PIBT + flow guide path | TPG | 381 | 491 | 353 | 1025 | 四域 `10000 tick` 统计已补齐；本轮完成 `city / game` 长时回归 |
| 迭代 04 | 10000 | Hungarian + task scoring | PIBT + flow guide path | TPG | 375 | 500 | - | - | 当前仅完成 `city / game`；相对基线分别 `-6 / +9` |

### 当前测试口径

- 单元测试：`ctest --test-dir build --output-on-failure`
- 统计回归：`./build/lifelong -s 1000|10000 -c 3 -d 3 --prettyPrintJson`
- 可视化仅在明确指定时使用 `-c 1` 生成 `*.viz.json`

### 当前已确认产物

- `result/city_paris_1_256_250.summary.json`
- `result/game_brc202d_500.summary.json`
- `result/random_32_32_20_100.summary.json`
- `result/warehouse_small_200.summary.json`
- `result/city_paris_1_256_250.10000.summary.json`
- `result/game_brc202d_500.10000.summary.json`
- `result/random_32_32_20_100.10000.summary.json`
- `result/warehouse_small_200.10000.summary.json`
- `result/city_paris_1_256_250.taskscore.10000.summary.json`
- `result/game_brc202d_500.taskscore.10000.summary.json`

### 说明

- 基线版本下，`city` 与 `game` 的 `makespan=10000` 已完成，结果分别为 `381` 和 `491`。这两个场景在当前默认参数下仍然接近实时推进：按默认 `-a 100`、`-t 1000`，`10000` tick 的 wall time 下界本身就接近 `1000s`，再叠加 planner 计算开销，完整跑完通常需要十几分钟。
- 开启 `LORR_ENABLE_TASK_SCORING=1` 后，本轮新增 `city / game` 长时结果分别为 `375` 和 `500`：`city` 相比基线下降 `6`，`game` 相比基线提升 `9`，说明当前评分函数在 `game` 上更有收益，`city` 仍需继续调参。
- 当前 planner 在 [src/MAPFPlanner.cpp](/Users/yxuanwkeith/Study/LoRR2026/src/MAPFPlanner.cpp#L37) 只要求最小 `2` 步 rollout，但每轮仍会执行一次 guide-path setup 和 flow 优化；对应热点集中在 [default_planner/planner.cpp](/Users/yxuanwkeith/Study/LoRR2026/default_planner/planner.cpp#L347)、[default_planner/flow.cpp](/Users/yxuanwkeith/Study/LoRR2026/default_planner/flow.cpp#L94) 和 [default_planner/search.cpp](/Users/yxuanwkeith/Study/LoRR2026/default_planner/search.cpp#L11)。
- 因此，`city / game` 的长时回归慢，主要不是 250 或 500 个 agent 的简单遍历成本，而是“长地图 + 长任务链 + 每轮近 1s 的 flow / A* / PIBT 计算预算 + 近实时模拟节拍”叠加导致。
