### LoRR2026 论文阅读笔记

> 文档定位：与当前代码结构和比赛设定直接相关的论文清单与阅读顺序。

这份清单不是泛泛而谈的 MAPF 书单，而是**按当前仓库的代码结构来筛论文**：

- `src/TaskScheduler.cpp` / `TaskManager.cpp`：在线任务分配、MAPD、终身任务流
- `default_planner/pibt.cpp`：PIBT / 优先级继承 / 局部冲突消解
- `default_planner/flow.cpp` / `search.cpp`：guide path、traffic flow、在线引导
- `src/Executor.cpp` / `src/Simulator.cpp`：执行层、delay、鲁棒性
- `src/CompetitionSystem.cpp`：滚动规划、有限时间预算、真实系统约束

### 1. 如果你只想先读最相关的 6 篇

按“**和这份 starter-kit 的贴合程度**”排序，我建议先读这 6 篇：

1. **Lifelong Multi-Agent Path Finding for Online Pickup and Delivery Tasks**
2. **Lifelong Multi-Agent Path Finding in Large-Scale Warehouses**
3. **Priority Inheritance with Backtracking for Iterative Multi-agent Path Finding**
4. **Traffic Flow Optimisation for Lifelong Multi-Agent Path Finding**
5. **Robust Multi-Agent Pickup and Delivery with Delays**
6. **Scaling Lifelong Multi-Agent Path Finding to More Realistic Settings: Research Challenges and Opportunities**

如果你的重点是：

- **任务分配 / 终身任务流**：优先读 1、2、6
- **默认 planner / `pibt.cpp`**：优先读 3、4
- **执行层 / `Executor` / delay**：优先读 5、6
- **想先补基础 MAPF 背景**：再读第 7~10 篇

### 2. 最贴近当前仓库的论文

### 2.1 终身任务与在线调度（最贴近 `TaskScheduler`）

- **Lifelong Multi-Agent Path Finding for Online Pickup and Delivery Tasks**
  - **作者 / 来源**：H. Ma, J. Li, S. Kumar, S. Koenig；AAMAS 2017（信息来自 `mapf.info` 公开论文页）
  - **链接**：[MAPF 社区论文页](http://mapf.info/index.php/Main/Publications)
  - **为什么相关**：这是把 **MAPD / 在线 pickup-and-delivery** 明确形式化的经典起点，非常适合对应你现在这套系统里的：
    - 持续补任务
    - agent 完成后重新接任务
    - `TaskScheduler` 需要在 ongoing task 池里做分配
  - **建议带着什么问题去读**：
    - 任务是如何在线到达的？
    - 任务分配和路径规划如何解耦？
    - token passing / task swap 这类思想能不能迁移到这份 starter-kit？

- **Lifelong Multi-Agent Path Finding in Large-Scale Warehouses**
  - **作者 / 来源**：Jiaoyang Li, Andrew Tinka, Scott Kiesel, Joseph W. Durham, T. K. Satish Kumar, Sven Koenig；AAAI 2021；[arXiv:2005.07371](https://arxiv.org/abs/2005.07371)
  - **核心关键词**：RHCR、rolling horizon、warehouse、lifelong MAPF
  - **为什么相关**：这篇和当前比赛/仓储场景非常近，重点是把终身 MAPF 分解成一系列**窗口化**问题来解。
  - **和代码的对应关系**：
    - 对应 `CompetitionSystem.cpp` 里的**慢环重规划**
    - 对应 `planCommTimeLimit` 这种**周期性通信/规划预算**
    - 对应“先给一段动作，再边执行边重新规划”的整体框架
  - **值得重点看**：
    - 滚动窗口怎么设
    - 规划 horizon 怎么影响吞吐
    - 为什么大规模仓储里要接受“局部看未来，而不是一次看完全部”

- **Scaling Lifelong Multi-Agent Path Finding to More Realistic Settings: Research Challenges and Opportunities**
  - **作者 / 来源**：He Jiang, Yulun Zhang, Rishi Veerapaneni, Jiaoyang Li；[arXiv:2404.16162](https://arxiv.org/abs/2404.16162)
  - **核心关键词**：League of Robot Runners、realistic settings、拥堵、短视、执行不确定性
  - **为什么相关**：这篇直接贴近 **League of Robot Runners / 更真实 LMAPF** 设定，讨论的问题几乎就是你现在会遇到的问题。
  - **和代码的对应关系**：
    - `CompetitionSystem.cpp` 里的时间预算限制
    - `Executor.cpp` 里的执行层现实约束
    - `TaskScheduler` / `Planner` 在高密度下的拥堵和短视行为
  - **建议重点读**：文中总结的三个挑战：
    - 大规模 / 高密度
    - 拥堵 / 短视
    - 文献模型与真实系统之间的差距

### 2.2 默认 planner 核心：PIBT、guide path、traffic flow

- **Priority Inheritance with Backtracking for Iterative Multi-agent Path Finding**
  - **作者 / 来源**：Keisuke Okumura 等；IJCAI 2019；[PIBT 官方仓库](https://github.com/Kei18/pibt)
  - **核心关键词**：priority inheritance、backtracking、iterative MAPF
  - **为什么相关**：你仓库里的 `default_planner/pibt.cpp` 就是这条线的直接相关工作。它非常适合高频在线规划、局部冲突快速消解。
  - **和代码的对应关系**：
    - `default_planner/pibt.cpp`
    - `planner.cpp` 中每一步 rollout 的局部决策
    - priority 更新、阻塞传播、局部让路机制
  - **建议带着什么问题去读**：
    - 优先级继承是怎么避免死锁的？
    - 回溯触发条件是什么？
    - 为什么它适合终身 / 在线重规划场景？

- **Traffic Flow Optimisation for Lifelong Multi-Agent Path Finding**
  - **作者 / 来源**：Zhe Chen, Daniel Harabor, Jiaoyang Li, Peter J. Stuckey；[arXiv:2308.11234](https://arxiv.org/abs/2308.11234)
  - **核心关键词**：traffic flow、guide path、拥堵规避、lifelong MAPF
  - **为什么相关**：当前默认 planner 里已经有非常强的 **guide path + flow** 味道，这篇正好讲“为什么不能总走自由流最短路，而要提前规避拥堵”。
  - **和代码的对应关系**：
    - `default_planner/flow.cpp`
    - `default_planner/search.cpp`
    - `planner.cpp` 里先修 guide path、再做 PIBT rollout 的整体思路
  - **建议重点看**：
    - 顶点拥堵 / 反向拥堵怎么建模
    - guide heuristic 怎么影响 PIBT
    - 为什么这类方法能显著提升吞吐量

- **Online Guidance Graph Optimization for Lifelong Multi-Agent Path Finding**
  - **作者 / 来源**：Hongzhi Zang, Yulun Zhang, He Jiang, Zhe Chen, Daniel Harabor, Peter J. Stuckey, Jiaoyang Li；[arXiv:2411.16506](https://arxiv.org/abs/2411.16506)
  - **核心关键词**：guidance graph、online optimization、PIBT、动态交通模式
  - **为什么相关**：如果你后面想继续增强 `flow.cpp` / `planner.cpp` 的“全局引导能力”，这篇几乎就是直接参考对象。
  - **和代码的对应关系**：
    - `flow.cpp` 中的在线引导优化
    - `planner.cpp` 中“先建立引导，再局部执行”的框架
  - **建议重点看**：
    - 引导图如何在线更新
    - 为什么在线 guidance 能优于静态手工规则
    - 如何和 PIBT 组合

### 2.3 执行层、延迟与鲁棒性（最贴近 `Executor`）

- **Robust Multi-Agent Pickup and Delivery with Delays**
  - **作者 / 来源**：Giacomo Lodigiani, Nicola Basilico, Francesco Amigoni；[arXiv:2303.17422](https://arxiv.org/abs/2303.17422)
  - **核心关键词**：MAPD、delay、robustness、replanning
  - **为什么相关**：这篇直接打在你这套系统的执行层痛点上——**实际执行有 delay 时，如何减少反复重规划和执行崩溃**。
  - **和代码的对应关系**：
    - `src/Executor.cpp`
    - `src/Simulator.cpp`
    - delay 机制下的 `GO/STOP` 与 staged actions 消费
  - **建议重点看**：
    - delay 是怎么进模型的
    - 鲁棒 token passing 怎么减少 replanning 次数
    - 哪些思想能迁移到“执行层更保守的放行动作判断”

- **Learn to Follow: Decentralized Lifelong Multi-agent Pathfinding via Planning and Learning**
  - **作者 / 来源**：Alexey Skrynnik, Anton Andreychuk, Maria Nesterova, Konstantin Yakovlev, Aleksandr Panov；[arXiv:2310.01207](https://arxiv.org/abs/2310.01207)
  - **核心关键词**：decentralized、planning + learning、lifelong MAPF
  - **为什么相关**：虽然你当前 starter-kit 不是学习方法，但这篇很适合给 `Executor` / 局部策略层提供灵感：**是否可以把“当前一步怎么过”学成策略，而不是纯规则判定**。
  - **和代码的对应关系**：
    - `Executor::next_command(...)`
    - 局部避碰 / 实时放行的策略化
  - **建议怎么看**：
    - 先不要急着复现网络，先看它怎么把“路径引导”和“局部决策”拆开

### 3. 静态 MAPF 背景与强基线

这几篇不一定最贴当前比赛节奏，但如果你想把论文脉络补齐，它们非常值得读。

- **Multi-Agent Path Finding – An Overview**
  - **作者 / 来源**：Roni Stern；2019；Springer LNAI；[论文页](https://link.springer.com/10.1007/978-3-030-33274-7_6)
  - **为什么值得读**：最适合作为入门导航。你可以先用它快速建立：
    - 什么是经典 MAPF
    - 完备 / 最优 / 次优 / 不完备方法怎么分
    - 终身 MAPF / 非经典 MAPF 在整个版图中的位置

- **Cooperative Pathfinding**
  - **作者 / 来源**：David Silver；AIIDE 2005；[Semantic Scholar 条目](https://www.semanticscholar.org/paper/03ef7f3a962319a8d97cacb6afa5380948eba1be)
  - **为什么值得读**：这是早期经典文献，帮助理解**窗口化 / cooperative / reservation** 这一脉络，对理解后来的 rolling horizon 很有帮助。

- **Conflict-Based Search for Optimal Multi-Agent Pathfinding**
  - **作者 / 来源**：Guni Sharon, Roni Stern, Ariel Felner, Nathan R. Sturtevant；Artificial Intelligence 219, 2015；[ScienceDirect](https://www.sciencedirect.com/science/article/pii/S0004370214001386)
  - **为什么值得读**：CBS 是静态 MAPF 里最经典的最优框架之一。即使你最后不打算用它，也应该知道它为何成为主线基线。
  - **和当前项目的关系**：
    - 可作为“高质量但偏重计算”的对照系
    - 帮你理解为什么终身 / 大规模场景更偏向 PIBT、rolling horizon、启发式 flow，而不是全局最优方法

- **EECBS: A Bounded-Suboptimal Search for Multi-Agent Path Finding**
  - **作者 / 来源**：Jiaoyang Li, Wheeler Ruml, Sven Koenig；AAAI 2021；[arXiv:2010.01367](https://arxiv.org/abs/2010.01367)
  - **为什么值得读**：如果你想知道静态 MAPF 里“质量和速度折中”的强实用基线，这篇很重要。
  - **和当前项目的关系**：
    - 它不一定直接适合你当前终身在线设置
    - 但非常适合拿来作为“如果分解成短窗口静态 MAPF，底层 solver 可以多强”的参照

### 4. 这份代码可以怎样对应去读论文

### 4.1 如果你想改 `TaskScheduler`

建议顺序：

1. **Lifelong Multi-Agent Path Finding for Online Pickup and Delivery Tasks**
2. **Lifelong Multi-Agent Path Finding in Large-Scale Warehouses**
3. **Scaling Lifelong Multi-Agent Path Finding to More Realistic Settings...**

重点关注：

- 在线任务分配与路径规划如何耦合 / 解耦
- 任务池大小、拥堵和吞吐量的关系
- 是否要加入 rebalancing、预留机制、future awareness

### 4.2 如果你想改 `default_planner/pibt.cpp`

建议顺序：

1. **Priority Inheritance with Backtracking for Iterative Multi-agent Path Finding**
2. **Traffic Flow Optimisation for Lifelong Multi-Agent Path Finding**
3. **Online Guidance Graph Optimization for Lifelong Multi-Agent Path Finding**

重点关注：

- priority inheritance / backtracking 的触发与终止条件
- 如何减少局部最优与全局拥堵之间的冲突
- 是否可以把 guide path / guidance graph 做得更动态

### 4.3 如果你想改 `Executor.cpp`

建议顺序：

1. **Robust Multi-Agent Pickup and Delivery with Delays**
2. **Scaling Lifelong Multi-Agent Path Finding to More Realistic Settings...**
3. **Learn to Follow...**

重点关注：

- delay 下如何减少不必要的 replanning
- 执行层到底应该保守到什么程度
- `GO/STOP` 是否可以从纯规则，变成“规则 + 预测”或者“规则 + 学习”的混合决策

### 5. 我建议你下一步怎么用这份文献表

如果你的目标是**尽快把成绩做上去**，我建议不是按年代读，而是按下面顺序：

1. **先读** `Lifelong Multi-Agent Path Finding in Large-Scale Warehouses`
2. **再读** `Priority Inheritance with Backtracking for Iterative Multi-agent Path Finding`
3. **再读** `Traffic Flow Optimisation for Lifelong Multi-Agent Path Finding`
4. **然后补** `Robust Multi-Agent Pickup and Delivery with Delays`
5. **最后用** `Scaling Lifelong Multi-Agent Path Finding to More Realistic Settings...` 检查自己方案的短板

这样读的好处是：

- 先对上比赛问题设定
- 再看当前默认算法最像哪条路线
- 再看怎么处理拥堵
- 最后看真实执行和现实约束

### 6. 备注

- 这份列表优先选择了**和当前代码最相关**的论文，而不是把整个 MAPF 文献一股脑全列出来。
- 对于 `Lifelong Multi-Agent Path Finding for Online Pickup and Delivery Tasks` 这篇经典 MAPD 论文，我当前能稳定核到的是 `mapf.info` 上的公开条目与发表信息，因此这里用社区公开论文页作为入口。
- 如果你愿意，我下一步可以继续把这份 `paper.md` 扩成一版：
  - **每篇论文 5~10 行详细笔记**
  - 或者直接做成 **“论文 -> 代码改造点” 对照表**。
