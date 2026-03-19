### LoRR2026 总览笔记

> 文档定位：题目规则、系统语义、输入输出和执行流程的总入口。

> 这份笔记基于仓库中的 `README.md`、`Input_Output_Format.md`、`Prepare_Your_Submission.md`、`Evaluation_Environment.md`、`Submission_Instruction.md`、`Working_with_Preprocessed_Data.md`、`Debug_and_Visualise_Your_Planner.md` 以及部分源码行为整理而成。

### 1. 比赛在做什么

- **问题类型**：这是一个 **Lifelong Multi-Agent / Multi-Robot** 任务规划比赛。机器人团队在网格地图上持续接收并执行任务，不是一次性把所有机器人送到终点，而是在固定仿真时长内持续完成尽可能多的任务。
- **核心目标**：在给定仿真时长内，让机器人团队完成尽可能多的任务。
- **任务形式**：每个任务由一个或多个按顺序执行的地点组成（errands），必须 **按顺序访问**。
- **在线性**：任务不是一次性全部开放，而是按规则逐步 reveal。系统始终维护一个任务池，完成多少任务，就继续补入多少任务。
- **多层决策结构**：比赛不是单纯 MAPF，而是三层联动：
  - **Scheduler**：给机器人分配任务；
  - **Planner**：给机器人生成多步路径/动作计划；
  - **Executor**：在执行层面每个 tick 决定机器人是 `GO` 还是 `STOP`。

### 2. 题目的关键难点

- **不是传统离散冲突模型**：这里不采用标准的 vertex conflict / edge conflict 作为最终执行判据。
- **采用连续重叠安全检查**：机器人被建模为正方形安全区域，障碍也按实体方块处理；系统会在一个 tick 内检查连续运动是否发生几何重叠。
- **动作存在执行时长**：`FW`、`CR`、`CCR` 不是 1 tick 完成，而是要经过多个 execution ticks，进度通过 `counter` 跟踪。
- **存在运行时延迟**：系统支持通过 `delayConfig` 注入 delay；机器人在 delay tick 中会被迫等待，这会破坏原始规划节奏。
- **规划与执行异步**：规划不是每 tick 都重新做，而是周期性进行；如果规划过慢，仿真不会暂停，而会继续按已有 staged actions 或等待策略推进。

### 3. 地图、机器人、动作与任务模型

- **地图格式**：`octile` 网格地图。
- **地图符号**：
  - **`@` / `T`**：障碍；
  - **`.`**：可通行空地；
  - **`E`**：emitter point，可通行；
  - **`S`**：service point，可通行。
- **位置编码**：使用线性下标，公式为 `row * width + col`。
- **坐标系**：`row` 向下增大，`col` 向右增大。
- **机器人动作**：
  - **`FW`**：前进一步；
  - **`CR`**：顺时针转向；
  - **`CCR`**：逆时针转向；
  - **`W`**：等待；
  - **`NA`**：保留值，不应输出到计划里。
- **执行命令**：Executor 每个 tick 输出 `GO` / `STOP`，系统再结合 staged actions、延迟与安全约束执行。
- **任务约束**：
  - 任务可由多个地点串成；
  - 只有当任务的第一个 errand 尚未完成时，任务才可被重新分配；
  - 一旦任务“打开”（即已完成首个 errand），就不能再转移给其他机器人。

### 4. 输入文件与输出文件

### 4.1 输入问题文件

输入主文件是一个 JSON，关键字段包括：

- **`mapFile`**：地图文件路径；
- **`agentFile`**：机器人起点文件；
- **`taskFile`**：任务文件；
- **`teamSize`**：机器人数量；
- **`numTasksReveal`**：任务池开放倍数，系统维持 `numTasksReveal × teamSize` 个已 reveal 任务；
- **`agentSize`**：机器人安全方形边长；
- **`maxCounter`**：一次 `FW/CR/CCR` 动作需要多少个 execution ticks；
- **`delayConfig`**：延迟生成参数，如随机种子、延迟模型、持续时间模型等。

`taskFile` 中每一行可以有多个位置，表示该任务要按顺序完成的一串 errands。

### 4.2 输出结果文件

程序输出 JSON，会记录：

- **`numTaskFinished`**：完成任务数；
- **`makespan`**：仿真总时长；
- **`actualPaths`**：真实执行路径；
- **`plannerPaths`**：规划器输出路径；
- **`events`**：任务推进/完成事件；
- **`errors`**：非法动作等 planner error；
- **`scheduleErrors`**：非法调度；
- **`plannerTimes`**：每次规划所用时间；
- **`delayIntervals`**：各机器人被 delay 的区间；
- **`numPlannerErrors`**、**`numScheduleErrors`**、**`numEntryTimeouts`**：错误与超时计数。

### 4.3 如何理解比赛目标

- **从文档表述看**：排行榜会根据评测结果计算一个 score。
- **从输出字段和源码行为看**：`numTaskFinished` 是最直接、最核心的性能摘要；此外错误数和超时数也会被输出。
- **一个重要现实判断**：仓库文档**没有公开写明完整 leaderboard 计分公式**，但本地调试和方案比较时，应该优先盯住：
  - **完成任务数是否上升**；
  - **`numPlannerErrors` / `numScheduleErrors` / `numEntryTimeouts` 是否为 0 或尽量低**；
  - **实际执行路径是否稳定，是否因 delay 导致严重拥堵**。

### 4.4 评测到底是“做完所有任务”还是“固定时间看完成数”

这是一个非常关键的决策点，结论是：

- **评测主逻辑是固定仿真时长，不是把所有任务做完才停止**；
- 系统主循环的停止条件是 **当前 timestep 小于 `simulationTime`**；
- 输出结果里的 `makespan` 直接就是本次设定的 `simulationTime`；
- 因此，评测核心是：**在固定时间窗口内，尽量把 `numTaskFinished` 做高**。

进一步说，这个比赛甚至不是“把 `taskFile` 里的有限任务全部清空”的玩法：

- 系统会持续维护一个已 reveal 的任务池；
- 每完成任务后，会继续 reveal 新任务补回任务池；
- 从当前 `TaskManager::reveal_tasks()` 的实现看，新任务是通过 `task_id % tasks.size()` 从任务模板中循环取出的。

这意味着：

- **当前 starter-kit 实现里，任务会持续补充，近似一个长期在线吞吐问题**；
- 所以你的优化目标应当理解为：**固定时间内最大化任务完成条数**，而不是“尽快把一批静态任务全部清掉”。

### 5. 系统是怎么调用你的算法的

这套系统是一个 **双速率控制环（two-rate loop）**。

### 5.1 慢环：规划更新

系统周期性执行以下过程：

- 同步最新环境到 `SharedEnvironment`；
- 调用 `Entry::compute(time_limit, proposed_plan, proposed_schedule)`；
- 默认 `Entry` 内部先调 `TaskScheduler::plan()`，再更新 `env->goal_locations`，然后调 `MAPFPlanner::plan()`；
- 返回后，系统会调用 **Executor** 处理新计划，把多步计划转成可执行的 `staged_actions`。

### 5.2 快环：逐 tick 执行

每一个 execution tick，系统都会：

- 调用 `Executor::next_command(...)` 产生每个机器人的 `GO/STOP`；
- 结合 staged actions 形成本 tick 请求动作；
- 施加 delay；
- 做连续几何安全检查；
- 推进一步仿真并更新状态与任务进度。

### 5.3 这意味着什么

- **规划慢了，仿真不会停**；
- **Executor 很关键**：即便 Planner 给出了多步动作，真正每 tick 能不能走、谁先走、谁停，还是要靠 Executor；
- **带 delay 的执行不等于按 planner path 原样播放**。

### 5.4 Planner 和 Executor 到底什么时候做决策

这个问题最容易混淆。可以直接记成：

- **Planner / Scheduler**：慢环，**周期性** 决策；
- **`Executor::next_command()`**：快环，**每个 execution tick** 都决策一次；
- **`Executor::process_new_plan()`**：每当有一版新的 planner 输出准备接入执行时调用一次。

更具体地说：

#### A. 仿真开始前后的第一次规划

初始化完成后，系统会先：

- reveal 初始任务；
- 同步 `SharedEnvironment`；
- 立即启动 **第一次 Planner 调用**；
- 这次调用用的是 **`initialPlanTimeLimit`**。

如果第一次规划没有在这个时间内返回：

- 仿真**不会等它**；
- 系统会继续推进执行 tick；
- 由于这时还没有新的 staged plan 可用，机器人会表现为等待或沿用已有安全执行逻辑。

#### B. 正常运行时，Planner 何时再次被调用

在主循环里，只有同时满足下面两个条件，系统才会开启下一次规划：

- **上一轮 Planner 已经结束**；
- **距离上一次规划启动，已经过了最小通信间隔 `planCommTimeLimit`**。

满足后，系统会按这个顺序做：

1. 先调用 **`Executor::process_new_plan(...)`**，把上一轮 Planner 产出的多步计划转成 `staged_actions`；
2. 再同步最新环境到 `env`；
3. 再启动新一轮 **`Entry::compute(...)`**；
4. 这时 Planner 使用的时间预算就是 **`planCommTimeLimit`**（更准确地说，`Entry::compute` 收到的 `time_limit` 就是这个慢环周期预算）。

#### B.1 Planner 输出计划的长度是固定的吗

结论要分两层看：

- **从接口层看**：`Plan` 本质上就是 `std::vector<std::vector<Action>>`，所以**计划长度不是框架写死的常量**，理论上可以由你的算法决定；
- **从当前 starter-kit 默认实现看**：默认 `MAPFPlanner` 会计算一个 **`min_plan_steps`**，然后调用 `DefaultPlanner::plan(limit, plan.actions, env, min_plan_steps)`，所以它当前输出的是一个**固定步数的多步计划**；
- 这个固定步数不是拍脑袋定的，而是按下面这个下界算出来的：
  - **`min_plan_steps = floor(planCommTimeLimit / (actionTime * max_counter)) + 1`**；
- 含义是：为了覆盖下一次 planner 通信回来之前的一段执行窗口，默认规划器至少会给出这么多步。

还要再注意一个实现细节：

- `Executor` 里虽然有 `APPEND_WINDOW` 和 `window_size` 的概念；
- 但当前 `Executor::process_new_plan(...)` 的实现，实际上是 **按 `plan[0].size()` 把整版计划全部 append 进去**，没有真的在这里截成固定 window。

所以更准确的理解是：

- **一次 Planner 调用的“运行时长”是固定预算的**（初次用 `initialPlanTimeLimit`，之后通常用 `planCommTimeLimit`）；
- **一次 Planner 产出的“动作序列长度”在接口上是可变的**；
- **但当前默认 planner 的实现，确实会按一个由系统参数推导出来的固定步数来产出计划**。

所以：

- **Planner 不是每 tick 都调用**；
- 它是在“上一轮结束 + 通信周期到点”之后，才重新规划一次。

#### C. Executor 什么时候做决策

Executor 有两个决策入口：

- **`process_new_plan(...)`**：
  - 触发时机：**每次新 planner 结果要落地时**；
  - 作用：把 `Plan` 转成未来一段时间可执行的 `staged_actions`，并生成预测状态，供下一轮规划使用。

- **`next_command(...)`**：
  - 触发时机：**每一个 execution tick 都会调用一次**；
  - 作用：针对每个机器人输出 `GO/STOP`；
  - 然后系统再结合 `staged_actions`、delay、安全检查，决定这一 tick 机器人实际执行什么动作。

也就是说：

- **Planner 决定“接下来一段路大概怎么走”**；
- **Executor 决定“这一 tick 现在能不能走、谁先走、谁要停”**。

#### D. 你应该如何理解这两个时间尺度

可以把它理解成两个不同层面的控制：

- **慢环（Planner / Scheduler）**：每隔一段时间重新看全局，更新任务分配与多步路径；
- **快环（Executor）**：每个 tick 根据现实执行状态、延迟和局部依赖关系做即时放行/阻塞。

所以在这个比赛里：

- 如果你做的是 **Planner**，你的核心问题是：**在有限慢环预算内，产出足够长、足够稳、足够好执行的多步计划**；
- 如果你做的是 **Executor**，你的核心问题是：**在每 tick 的极短预算内，做出鲁棒的实时执行决策**。

### 6. 三个赛道分别让你改什么

根据文档说明：

- **Scheduling Track**：实现你自己的 `TaskScheduler`，Planner 和 Executor 用默认实现；
- **Execution Track**：实现你自己的 `Executor`，任务分配和规划用默认实现；
- **Combined Track**：同时实现 Scheduler、Planner、Executor，必要时也可以改 `Entry`，但不能改 API 签名。

### 7. 哪些文件不能乱改

评测时服务器会：

- 克隆你指定分支；
- **覆盖所有受保护、不可修改的 start-kit 文件**；
- 在 Docker 中编译和评测。

所以：

- **即使你本地改了某些受保护文件，评测时也会被覆盖掉**；
- 更重要的是，文档明确要求你不能通过任何直接或间接方式干扰这些受保护功能。

重点是：

- **大多数 `src/`、`inc/` 中的系统核心文件不能动**；
- 不同赛道下，`MAPFPlanner` / `TaskScheduler` / `Executor` / `Entry` 里可改范围也不同；
- 提交前一定要对照 `Evaluation_Environment.md` 里的 protected file 列表检查。

### 8. 时间限制与超时机制

比赛里有多种时间预算：

- **预处理时间**：`initialize()` 阶段使用，属于硬限制；
- **初始规划时间**：第一次规划调用的预算；
- **规划通信间隔**：两次慢环规划之间最小间隔；
- **每 tick 执行时间**：Executor 每个 tick 的预算；
- **新计划处理时间**：Executor 接收与消化新计划的预算。

### 8.1 预处理

文档说明：

- **每张地图允许最多 30 分钟预处理**；
- 如果 `initialize()` 超时，程序终止，退出码为 `124`。

### 8.2 在线超时

- 规划慢了：系统继续执行旧的 staged actions；
- `next_command()` 太慢：会挤占执行预算；
- `process_new_plan()` 太慢：新计划不能及时生效。

**结论**：

- `next_command()` 必须尽量轻；
- 慢环可以复杂，但必须稳定返回；
- 单纯提高计划质量却频繁超时，可能会得不偿失。

### 9. 本地编译与测试方法

### 9.1 编译

仓库推荐两种方式：

- **脚本**：`./compile.sh`
- **手动**：

```bash
mkdir build
cmake -B build ./ -DCMAKE_BUILD_TYPE=Release
make -C build -j
```

当前仓库中的 `compile.sh` 默认编译 C++ 版本，目标产物必须是：

- **`build/lifelong`**

这点非常关键，因为评测系统就是查找并执行这个文件。

### 9.2 本地运行

基本命令：

```bash
./build/lifelong --inputFile ./example_problems/random.domain/random_32_32_20_100.json -o test.json
```

常用参数：

- **`--inputFile` / `-i`**：输入 JSON，必填；
- **`--output` / `-o`**：输出 JSON 路径；
- **`--prettyPrintJson`**：美化输出 JSON；
- **`--outputScreen` / `-c`**：输出详细程度；
- **`--logFile` / `-l`**：日志文件；
- **`--logDetailLevel` / `-d`**：日志级别；
- **`--simulationTime` / `-s`**：仿真总 ticks；
- **`--preprocessTimeLimit` / `-p`**：预处理时间；
- **`--initialPlanTimeLimit` / `-n`**：初始规划预算；
- **`--planCommTimeLimit` / `-t`**：慢环规划最小通信间隔；
- **`--actionMoveTimeLimit` / `-a`**：每个 execution tick 的预算；
- **`--executorProcessPlanTimeLimit` / `-x`**：新计划处理预算；
- **`--fileStoragePath` / `-f`**：大文件存储目录。

### 9.3 当前代码里的默认值

从 `src/driver.cpp` 能确认当前默认值：

- **`actionMoveTimeLimit`**：100 ms
- **`outputScreen`**：1
- **`initialPlanTimeLimit`**：1000 ms
- **`preprocessTimeLimit`**：30000 ms
- **`simulationTime`**：5000
- **`planCommTimeLimit`**：1000 ms
- **`outputActionWindow`**：100
- **`executorProcessPlanTimeLimit`**：100 ms

### 9.4 本地测试建议

推荐你至少做三类测试：

- **功能正确性测试**：先在 `example_problems` 下的小例子验证是否有错误；
- **时限压力测试**：缩小 `time_limit`，观察超时与性能退化；
- **扰动鲁棒性测试**：打开带 `delayConfig` 的实例，观察实际路径与计划路径偏差。

### 10. 如何调试

调试主要依赖输出 JSON：

- **看 `errors`**：是否有无效动作；
- **看 `scheduleErrors`**：是否有非法任务分配；
- **看 `plannerPaths` 与 `actualPaths`**：计划是否能稳定落地；
- **看 `plannerSchedule` 与 `actualSchedule`**：调度策略是否真的被执行；
- **看 `plannerTimes`**：是否超时/抖动明显；
- **看 `events` 与 `tasks`**：哪些任务推进顺利，哪些卡住。

此外可使用 **PlanViz** 可视化输出 JSON，查看机器人动画、错误时刻和任务事件。

### 11. Docker 测试方法

官方评测是在 Docker 里完成的，建议本地尽量复现。

直接运行：

```bash
./RunInDocker.sh
```

它会：

- 基于官方镜像自动生成 Dockerfile；
- 安装 `apt.txt` 和 `pip.txt` 中的依赖；
- 复制代码进容器；
- 执行 `compile.sh`；
- 进入容器环境。

如果你是 **Mac ARM** 且只是本地无 GPU 调试，文档建议可改用：

```bash
./RunInDocker.sh ubuntu:jammy
```

容器默认信息：

- **镜像名**：`mapf_image`
- **容器名**：`mapf_test`
- **工作目录**：`/MAPF/codes/`

在容器外运行程序示例：

```bash
docker container exec mapf_test ./build/lifelong --inputFile ./example_problems/random.domain/random_20.json -o test.json
```

把结果拷回宿主机：

```bash
docker cp mapf_test:/MAPF/codes/test.json ./test.json
```

### 12. 评测环境与服务器条件

官方评测环境大致为：

- **CPU**：x86_64，32 vCPU
- **内存**：128 GB
- **磁盘可用空间**：约 27 GB
- **GPU**：NVIDIA A10G
- **系统**：Ubuntu 22.04.3 LTS
- **Docker 基础镜像**：`pytorch/pytorch:2.4.1-cuda11.8-cudnn9-devel`
- **网络**：**无互联网访问**

因此：

- 不要依赖联网下载；
- 如果你用模型文件、预处理索引或大依赖，要提前放到允许的位置；
- 如果要用第三方库，要么放进仓库，要么通过 `apt.txt` / `pip.txt` 安装。

### 13. 大文件与离线预处理怎么处理

如果你的方法需要：

- 预训练模型；
- 大型启发式表；
- 离线生成的索引；
- 编译时大依赖；

可以使用比赛提供的 **Large File Storage**。

使用方式：

- 网站 `My Submission` 页面底部有 **Large File Storage**；
- 上传的大文件会在评测开始时同步到服务器；
- 容器里会以只读方式挂载该目录；
- 运行时可以通过：
  - **命令行 `--fileStoragePath`** 指定；或
  - **环境变量 `LORR_LARGE_FILE_STORAGE_PATH`** 获取。

程序中可以从 `env->file_storage_path` 读到该路径。

### 14. 提交方法

提交流程如下：

- 用 GitHub 登录比赛网站；
- 系统会给你创建一个 **私有 submission repo**；
- 在本地修改、测试后，执行：

```bash
git add [modified/new created files]
git commit -m "your message"
git push origin
```

然后在官网：

- 进入 **My Submission** 页面；
- 在 **Evaluate the Branch** 里选择分支；
- 点击 **Start Evaluation** 启动评测；
- 在 **My Submissions** 面板中查看运行进度和历史结果。

### 15. 官方评测服务器到底会做什么

每次评测时，服务器会：

- 克隆你指定分支；
- 覆盖受保护的不可修改文件；
- 构建 Docker 环境；
- 执行 `compile.sh` 编译；
- 运行评测并计算排行榜分数。

### 16. 参赛时最值得盯住的几个指标

我建议把下面这些作为调参主指标：

- **`numTaskFinished`**：第一优先级；
- **`numPlannerErrors`**：必须尽量压低；
- **`numScheduleErrors`**：调度非法会直接伤害效果；
- **`numEntryTimeouts`**：超时会让系统继续跑旧计划或等待；
- **`plannerTimes`**：看你是否在时限边缘抖动；
- **`actualPaths` vs `plannerPaths`**：看 delay 与执行器是否造成严重偏离。

### 17. 默认基线算法值得怎么理解

默认实现并不是随便写的 baseline：

- **默认 Scheduler**：贪心地给空闲机器人分配当前估计 makespan 最小的任务；
- **默认 Planner**：基于 Traffic Flow Optimised Guided PIBT；
- **默认 Executor**：基于 Temporal Dependency Graph 决定每 tick 的 `GO/STOP`。

这意味着：

- 如果你只打某一个赛道，你其实是在和另外两个默认模块配合；
- 你的模块不一定要“全能”，但要与默认模块的接口习惯匹配；
- 对 Scheduling Track 来说，**调度返回越快，默认 planner 可用时间越多**。

### 18. 我认为这道题的本质

这题的本质不是“单次最短路”，而是：

- **在线任务分配**
- **受扰动的多机器人协同规划**
- **延迟鲁棒执行控制**
- **在固定时长内最大化吞吐量**

如果只追求静态路径最优，通常不够；真正决定成绩的是：

- 调度是否减少拥堵与资源争用；
- 规划是否能给 Executor 留出可执行余地；
- 执行策略是否能在 delay 下持续消化计划。

### 19. 文档与代码里值得注意的几个坑

- **排行榜 score 公式未在仓库文档中明确公开**：只能从输出字段与评测流程推断调参重点。
- **`sumOfCost` 在 `Input_Output_Format.md` 中有列出，但当前代码保存结果时未见对应字段写出**：说明文档与当前实现可能存在轻微滞后。
- **`outputActionWindow` 参数在文档中可配置，但当前 `src/driver.cpp` 里调用 `simulate(...)` 时实际传入的是固定值 `100`**：本分支下该参数并未真正生效。
- **预处理时间有“文档与本地默认值”差异**：文档写官方评测允许 **30 分钟**，但本地 `driver.cpp` 默认 `--preprocessTimeLimit` 是 **30000 ms**。因此本地若要模拟官方环境，建议显式传参，而不要依赖默认值。
- **Python 接口当前阶段不支持正式提交**：现阶段仍应以 C++ 提交链路为准。

### 20. 一个实用的本地测试命令模板

如果你想尽量接近“认真调试”模式，可以从下面这个命令开始：

```bash
./build/lifelong \
  --inputFile ./example_problems/random.domain/random_32_32_20_100.json \
  --output ./debug_output.json \
  --prettyPrintJson \
  --outputScreen 1 \
  --simulationTime 5000 \
  --preprocessTimeLimit 1800000 \
  --initialPlanTimeLimit 1000 \
  --planCommTimeLimit 1000 \
  --actionMoveTimeLimit 100 \
  --executorProcessPlanTimeLimit 100
```

### 21. 一句话总结

- **题目本质**：在带延迟、带连续安全约束、规划执行异步的多机器人系统里，最大化长期任务吞吐量。
- **本地测试重点**：看 `numTaskFinished`、错误数、超时数，以及 `actualPaths` 是否稳定。
- **提交重点**：保证 `compile.sh` 能产出 `build/lifelong`，不要依赖受保护文件修改，提交后去官网指定分支发起评测。
