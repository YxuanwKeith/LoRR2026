from __future__ import annotations

import json
from datetime import date
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RESULT_DIR = ROOT / "result"

CASES = [
    (
        "city",
        "paris_1_256_250.json",
        "example_problems/city.domain/maps/Paris_1_256.map",
        "city_paris_1_256_250.viz.json",
    ),
    (
        "game",
        "brc202d_500.json",
        "example_problems/game.domain/maps/brc202d.map",
        "game_brc202d_500.viz.json",
    ),
    (
        "random",
        "random_32_32_20_100.json",
        "example_problems/random.domain/maps/random-32-32-20.map",
        "random_32_32_20_100.viz.json",
    ),
    (
        "warehouse",
        "warehouse_small_200.json",
        "example_problems/warehouse.domain/maps/warehouse_small.map",
        "warehouse_small_200.viz.json",
    ),
]


def load_json(path: Path) -> dict:
    return json.loads(path.read_text())


def main() -> None:
    lines = [
        "### 本地回归测试整理",
        "",
        f"- 生成日期: {date.today().isoformat()}",
        "- 运行方式: `result/run_local_regression.sh`",
        "- 仿真参数: `-s 1000 -c 1 --prettyPrintJson`",
        "- CTest 日志: `result/ctest.log`",
        "",
        "| 测试集 | 样例 | teamSize | numTaskFinished | makespan | numPlannerErrors | numScheduleErrors | numEntryTimeouts | 可视化文件 |",
        "| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
    ]

    planviz_lines = [
        "### PlanViz 启动方案",
        "",
        "先准备 Python 3.11 环境并安装依赖:",
        "",
        "```bash",
        "python3.11 -m venv .venv-planviz",
        "source .venv-planviz/bin/activate",
        "pip install -r PlanViz/requirements.txt",
        "```",
        "",
        "PlanViz 是单窗口交互工具，每次只打开一个场景。按需执行其中一条命令:",
        "",
        "```bash",
    ]

    for domain, sample, map_path, output_name in CASES:
        data = load_json(RESULT_DIR / output_name)
        lines.append(
            "| {domain} | `{sample}` | {team} | {finished} | {makespan} | {planner_err} | {sched_err} | {timeout} | `result/{output}` |".format(
                domain=domain,
                sample=sample,
                team=data.get("teamSize", 0),
                finished=data.get("numTaskFinished", 0),
                makespan=data.get("makespan", 0),
                planner_err=data.get("numPlannerErrors", 0),
                sched_err=data.get("numScheduleErrors", 0),
                timeout=data.get("numEntryTimeouts", 0),
                output=output_name,
            )
        )
        planviz_lines.append(
            "# {domain}\npython3.11 PlanViz/script/run.py --map {map_path} --plan result/{output_name}".format(
                domain=domain,
                map_path=map_path,
                output_name=output_name,
            )
        )

    planviz_lines.extend(
        [
            "```",
            "",
            "也可以直接使用 `result/open_planviz.sh <city|game|random|warehouse>`。",
            "",
        ]
    )

    (RESULT_DIR / "local_regression.md").write_text("\n".join(lines + [""] + planviz_lines), encoding="utf-8")


if __name__ == "__main__":
    main()
