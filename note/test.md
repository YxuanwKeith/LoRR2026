# Test Rules

`note/test.md` 用于固定本地回归、结果归档和 PlanViz 可视化的统一规则，避免每轮迭代重复摸索测试口径。

## 测试目标

- 用同一套输入、同一套命令比较不同版本算法表现。
- 将构建、单测、四域样例回归结果统一落到 `result/`，默认不生成可视化产物。
- 保证 `result/summary.md`、`result/local_regression.md` 中的统计口径一致。

## 固定测试集

本地回归默认覆盖四个代表样例：

- `city`: `example_problems/city.domain/paris_1_256_250.json`
- `game`: `example_problems/game.domain/brc202d_500.json`
- `random`: `example_problems/random.domain/random_32_32_20_100.json`
- `warehouse`: `example_problems/warehouse.domain/warehouse_small_200.json`

只有在明确需要可视化排查时，才为这四个样例额外生成 PlanViz 输入文件。

## 测试分层

每次正式记录结果时，至少执行下面三层测试。

### 1. 构建与单元测试

- 构建命令:

```bash
cmake -S . -B build -DPYTHON=false -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

- 单测命令:

```bash
ctest --test-dir build --output-on-failure
```

- 日志落点:
  - `result/ctest.log`

### 2. 四域样例回归

- 当前默认回归脚本:

```bash
result/run_local_regression.sh
```

- 脚本内部固定行为:
  - 先重新配置并构建 `build/`
  - 再运行 `CTest`
  - 然后对四个样例依次运行 `./build/lifelong`
  - 默认只保留统计结果，不生成 `PlanViz` 专用 JSON

- 统计回归推荐参数:

```bash
-s 1000 -c 3 --prettyPrintJson
```

- 大图或长时回归推荐附加 `-d 3`，只保留 fatal 日志与最终结果，避免终端被 planner 调试输出淹没:

```bash
-s 1000 -c 3 -d 3 --prettyPrintJson
```

含义如下：

- `-s 1000`: 仿真 horizon 为 1000 tick
- `-c 3`: 仅输出 summary 统计，适合做版本对比
- `--prettyPrintJson`: 便于人工阅读与后处理

- 只有在需要可视化时，才切换到:

```bash
-s 1000 -c 1 --prettyPrintJson
```

此时才会输出 PlanViz 所需路径、调度与事件。

### 当前人工串行运行方案

当前结果统一整理在 `result/` 中，并采用 **先跑 `1000 tick`，再跑 `10000 tick`** 的串行执行方式。

串行的含义是：

- 同一时刻只跑一个 `./build/lifelong`
- 前一条命令结束后，再启动下一条
- 不并发、不后台混跑，避免多个实验同时写入 `result/` 干扰日志与结果归档

以 `city` 场景 `example_problems/city.domain/paris_1_256_250.json` 为例，推荐固定为下面两条命令。

- `1000 tick` 统计命令：

```bash
./build/lifelong -i ./example_problems/city.domain/paris_1_256_250.json -o ./result/city_paris_1_256_250.summary.json -s 1000 -c 3 -d 3 -l ./result/city_1000.log --progressInterval 100 --progressFile ./result/city_1000.progress.log --prettyPrintJson
```

- `10000 tick` 统计命令：

```bash
./build/lifelong -i ./example_problems/city.domain/paris_1_256_250.json -o ./result/city_paris_1_256_250.10000.summary.json -s 10000 -c 3 -d 3 -l ./result/city_10000.log --progressInterval 100 --progressFile ./result/city_10000.progress.log --prettyPrintJson
```

参数口径固定如下：

- `-i`: 输入场景文件
- `-o`: 最终 summary 输出路径
- `-s`: 仿真 tick 数，当前固定比较 `1000` 与 `10000`
- `-c 3`: 只保留 summary 统计
- `-d 3`: 只输出 fatal 级别日志，减少长跑噪声
- `-l`: 主运行日志文件
- `--progressInterval 100`: 每 100 tick 写一次进度
- `--progressFile`: 进度日志文件
- `--prettyPrintJson`: 便于后续人工查看与整理

如果后续扩展到 `game`、`random`、`warehouse`，保持完全相同的参数口径，只替换：

- 输入实例 `-i`
- 输出 summary 文件名 `-o`
- 对应场景的 `log / progress` 文件名前缀

### 3. 可视化验证

- 可视化不是默认测试步骤，而是按需调试步骤。
- 只有在指定要看动画、路径、事件或调度细节时，才生成 `*.viz.json`。
- PlanViz 使用 `PlanViz/.venv` 中的 Python 环境。
- 每次只打开一个场景窗口，不同时启动多个可视化实例。
- 快捷命令:

```bash
result/open_planviz.sh city
result/open_planviz.sh game
result/open_planviz.sh random
result/open_planviz.sh warehouse
```

## 结果文件约定

### 默认统计产物

- `result/ctest.log`
- `1000 tick` 的 summary 文件，例如:
  - `result/city_paris_1_256_250.summary.json`
  - `result/game_brc202d_500.summary.json`
  - `result/random_32_32_20_100.summary.json`
  - `result/warehouse_small_200.summary.json`
- `10000 tick` 的 summary 文件，例如:
  - `result/city_paris_1_256_250.10000.summary.json`
  - `result/random_32_32_20_100.10000.summary.json`
  - `result/warehouse_small_200.10000.summary.json`
- 若保留运行过程文件，则按场景前缀落到 `result/`:
  - `result/city_1000.log`
  - `result/city_1000.progress.log`
  - `result/city_10000.log`
  - `result/city_10000.progress.log`
  - `result/city_10000.pid`
  - `result/city_10000.exitcode`

### 按需可视化产物

- `result/city_paris_1_256_250.viz.json`
- `result/game_brc202d_500.viz.json`
- `result/random_32_32_20_100.viz.json`
- `result/warehouse_small_200.viz.json`

这些文件只在明确要求可视化时生成。

### 汇总文档

- `result/local_regression.md`
  - 面向当前一次本地回归结果的简表。
  - 若该轮未生成可视化文件，则不应把 PlanViz 命令当作默认步骤写进去。
- `result/summary.md`
  - 面向多轮版本迭代的长期记录。

## 统计口径

记录版本表现时，优先保留下列指标：

- 算法版本信息
- `makespan`
- `city` 的 `numTaskFinished`
- `game` 的 `numTaskFinished`
- `random` 的 `numTaskFinished`
- `warehouse` 的 `numTaskFinished`

默认不在主表中保留以下字段，除非该轮迭代专门在排查稳定性：

- `numPlannerErrors`
- `numScheduleErrors`
- `numEntryTimeouts`

原因是这些字段当前多数版本都为 0，长期保留会压缩主表的可读性，不利于横向比较算法收益。

## makespan 规则

后续版本对比采用双 horizon 口径，每个版本固定做两轮：

1. `makespan = 1000`
2. `makespan = 10000`

记录要求：

- 同一个算法版本在汇总表中占两行。
- 第二列放 `makespan`。
- 一行内同时展示四个域的 `numTaskFinished`。

推荐表头如下：

| 版本 | makespan | 任务分配 | 路径规划 | 执行决策 | city | game | random | warehouse | 备注 |
| --- | ---: | --- | --- | --- | ---: | ---: | ---: | ---: | --- |

示意：

| 迭代 03 | 1000 | Hungarian | PIBT | TPG | x | x | x | x |  |
| 迭代 03 | 10000 | Hungarian | PIBT | TPG | x | x | x | x | 长 horizon |

## 建议执行顺序

一次完整版本评估建议按下面顺序做：

1. 完成代码修改。
2. 运行 `ctest --test-dir build --output-on-failure`。
3. 跑 `makespan=1000` 的四域回归。
4. 若需要定位行为问题，再额外生成可视化并抽查 1 到 2 个场景的 PlanViz。
5. 再跑 `makespan=10000` 的四域回归。
6. 更新 `result/summary.md` 主表。

## 注意事项

- 比较不同版本时，输入样例、`makespan`、`outputScreen` 必须一致。
- 只做统计时，优先使用 `-c 3`，减少输出体积与整理成本。
- 大图或 `makespan=10000` 的回归优先加 `-d 3`，只输出最终结果。
- 只有明确要可视化时，才使用 `-c 1` 生成 `*.viz.json`。
- 若要追加新版本统计，优先在 `result/summary.md` 主表更新，不要只改 `local_regression.md`。
- 若某轮引入新的测试样例，必须在本文同步补充，避免口径漂移。
