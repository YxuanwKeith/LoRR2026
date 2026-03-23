# 消融实验方案

## 一、参数调整方式

采用 **"编译期默认 + 环境变量运行时覆盖"** 双层机制，**调参不需要重新编译**：

- `optsvr/Config.h`：`constexpr` 编译期默认值（提交评测时使用）
- `optsvr/RuntimeConfig.h`：运行时从 `OPT_*` 环境变量加载，不传则回退到默认值

```
编译一次 → 通过环境变量切换参数 → 直接运行 → 下一组参数
```

### 方式一：手动通过环境变量调参

```bash
# 使用默认参数直接跑
./build/lifelong -i example_problems/warehouse.domain/warehouse_small_200.json -s 1000

# 覆盖单个参数
OPT_DIFFICULTY_WEIGHT=0.5 \
  ./build/lifelong -i example_problems/warehouse.domain/warehouse_small_200.json -s 1000

# 覆盖多个参数
OPT_HUNGARIAN_THRESHOLD=300 OPT_DIFFICULTY_WEIGHT=0.5 \
  ./build/lifelong -i example_problems/warehouse.domain/warehouse_small_200.json -s 1000

# 关闭优化跑 baseline
OPT_USE_OPT_SCHEDULER=0 \
  ./build/lifelong -i example_problems/warehouse.domain/warehouse_small_200.json -s 1000
```

### 方式二：快速单次测试脚本

```bash
# 使用默认参数
./result/run_quick_test.sh

# 指定场景和步数
./result/run_quick_test.sh --scene city --steps 2000

# 覆盖参数（自动加 OPT_ 前缀）
./result/run_quick_test.sh --env DIFFICULTY_WEIGHT=0.5
./result/run_quick_test.sh --env HUNGARIAN_THRESHOLD=300 --env DIFFICULTY_WEIGHT=0.5

# 跑 baseline
./result/run_quick_test.sh --baseline
```

### 方式三：批量消融实验

```bash
# 运行全部消融实验（一次编译，多次运行）
./result/ablation.sh

# 只运行 A 组（主开关消融：baseline vs 优化）
./result/ablation.sh --group A

# D 组（难度权重），2000 步
./result/ablation.sh --group D --steps 2000

# 指定测试场景
./result/ablation.sh --group A --scene city

# 先预览不实际运行
./result/ablation.sh --group B --dry-run
```

**优势**：
- ✅ 编译一次，跑任意多组参数
- ✅ 不修改源码，不需要备份还原 Config.h
- ✅ 提交评测时不设环境变量，自动使用默认值
- ✅ 环境变量方式对 CI/批量脚本非常友好

---

## 二、消融实验设计

### 实验目标

验证各模块的独立贡献，确定最优参数组合。

### 基准 (Baseline)

`USE_OPT_SCHEDULER = false`，即原始贪心最短距离分配。

### 实验组

#### 实验 A：主开关消融（优化调度器 vs Baseline）

| 组别 | USE_OPT_SCHEDULER | 说明 |
|------|-------------------|------|
| A0 | `false` | Baseline |
| A1 | `true` (其他默认) | 完整优化 |

#### 实验 B：匹配策略阈值消融

固定 `USE_OPT_SCHEDULER = true`，调整匈牙利/区域匹配切换阈值。

| 组别 | HUNGARIAN_THRESHOLD | 说明 |
|------|-------------------|------|
| B1 | 50 | 更多使用区域匹配 |
| B2 | 100 | |
| B3 | 200 (默认) | |
| B4 | 500 | 更多使用匈牙利 |
| B5 | 9999 | 强制全部匈牙利 |

#### 实验 C：拥塞热力图消融

固定其他默认参数，调整拥塞相关参数。

| 组别 | CONGESTION_DECAY | CONGESTION_TTL | 说明 |
|------|-----------------|----------------|------|
| C0 | 0.0 | 0 | 禁用拥塞（权重清零） |
| C1 | 0.7 | 50 | 快速衰减，短时效 |
| C2 | 0.9 | 100 (默认) | |
| C3 | 0.95 | 200 | 慢衰减，长时效 |
| C4 | 0.99 | 500 | 接近无衰减 |

#### 实验 D：任务难度权重消融

| 组别 | DIFFICULTY_WEIGHT | 说明 |
|------|------------------|------|
| D0 | 0.0 | 纯距离适配度（无难度） |
| D1 | 0.1 | |
| D2 | 0.3 (默认) | |
| D3 | 0.5 | 距离和难度各半 |
| D4 | 0.7 | |
| D5 | 1.0 | 纯任务难度 |

#### 实验 E：候选数量消融（大规模场景）

| 组别 | CANDIDATE_TOPK | 说明 |
|------|---------------|------|
| E1 | 10 | 极少候选 |
| E2 | 30 | |
| E3 | 50 (默认) | |
| E4 | 100 | |
| E5 | 200 | 大量候选 |

#### 实验 F：区域粒度消融

| 组别 | REGION_SIZE | 说明 |
|------|------------|------|
| F1 | 2 | 细粒度 |
| F2 | 4 (默认) | |
| F3 | 8 | 粗粒度 |
| F4 | 16 | 超粗粒度 |

---

## 三、测试场景选择

| 场景 | 文件 | Agent 数 | 特点 |
|------|------|---------|------|
| 小规模仓库 | `warehouse.domain/warehouse_small_200.json` | 200 | 快速验证 |
| 大规模仓库 | `warehouse.domain/warehouse_large_5000.json` | 5000 | 测试扩展性 |
| 分拣中心 | `warehouse.domain/sortation_large_2000.json` | 2000 | 中等规模 |
| 城市地图 | `city.domain/paris_1_256_250.json` | 250 | 开放地图 |
| 游戏地图 | `game.domain/brc202d_500.json` | 500 | 不规则地图 |
| 随机地图 | `random.domain/random_32_32_20_100.json` | 100 | 小型测试 |

### 推荐优先级

1. **快速迭代**：`warehouse_small_200` + `random_32_32_20_100`（编译 + 运行 < 1 分钟）
2. **中等验证**：`paris_1_256_250` + `brc202d_500`
3. **全量测试**：所有 6 个场景

---

## 四、评价指标

从程序输出中提取：

| 指标 | 说明 | 越高/低越好 |
|------|------|------------|
| `throughput` | 单位时间完成的任务数 | 越高越好 |
| 总完成任务数 | 在给定步数内完成的任务总数 | 越高越好 |
| 运行时间 | 每轮 plan 的平均/最大耗时 | 越低越好（必须 < 时限） |

---

## 五、结果记录格式

每次实验结果记录在 `result/ablation_results.md` 中，格式如下：

```markdown
## 实验 X：<实验名称>
日期：YYYY-MM-DD
场景：<场景名>

| 组别 | 参数值 | throughput | 总完成任务数 | 平均plan耗时(ms) | 最大plan耗时(ms) |
|------|--------|-----------|------------|-----------------|-----------------|
| X0   | ...    | ...       | ...        | ...             | ...             |
| X1   | ...    | ...       | ...        | ...             | ...             |
```
