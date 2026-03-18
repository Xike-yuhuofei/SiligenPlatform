# 数据资产布局

更新时间：`2026-03-18`

## 1. 目标

本布局用于把配方、schema、示例、回归基线收口到工作区根级 `data/` 与 `examples/`，同时保留迁移期兼容桥接，避免静默移动导致运行时找不到资源。

## 2. 分类规则

| 分类 | 定义 | canonical 落位 | 备注 |
|---|---|---|---|
| 源码资产 | 人工维护、被业务逻辑直接消费的正式数据 | `data/recipes/`、`data/schemas/`、`examples/` | 必须有 owner 和 canonical 路径 |
| 运行时业务产物 | 运行时产生，但仍属于业务记录的一部分 | `data/recipes/audit/` | 可保留 seed，不应被误判为日志 |
| 回归基线/派生夹具 | 用于契约比对、仿真回归的稳定样本 | `data/baselines/`、`examples/replay-data/` | 必须明确“可比对”或“演示”用途 |
| 日志/临时文件 | 构建、上传、缓存、日志、预览临时页 | 不进入 canonical | 例如 `build/`、`tmp/`、`__pycache__/`、`logs/` |

## 3. Canonical 目录与 owner

| 资产 | owner | canonical 路径 | 当前说明 |
|---|---|---|---|
| 配方主记录 | `control-core` 配方域 | `data/recipes/recipes/` | 来自 legacy recipe 仓储，已复制到根级 |
| 配方版本 | `control-core` 配方域 | `data/recipes/versions/` | 属于受控业务数据，不是临时文件 |
| 配方模板 | `control-core` 配方域 | `data/recipes/templates/` | 已从 `templates.json` 拆成每模板单文件 |
| 配方审计 seed | `control-core` 配方域 | `data/recipes/audit/` | 保留为业务审计 seed，非手工编辑主源 |
| 配方参数 schema | `control-core` 配方域 | `data/schemas/recipes/default.json` | runtime-host 已改为优先读取这里 |
| DXF proto / JSON schema | `packages/engineering-contracts` | `data/schemas/engineering/dxf/v1/` | 工作区公开 canonical，owner 副本仍保留在包内 |
| 工程二进制夹具 | `packages/engineering-contracts` | `data/baselines/engineering/rect_diag.pb` | 用于协议和导出链路回归 |
| 预览期望输出 | `packages/engineering-contracts` | `data/baselines/engineering/rect_diag.preview-artifact.json` | 属于回归基线，不是源码输入 |
| 仿真回归基线 | `packages/simulation-engine` + `integration` | `data/baselines/simulation/rect_diag.simulation-baseline.json` | 用于仿真结果稳定性比对 |
| DXF 输入样例 | `packages/engineering-contracts` | `examples/dxf/rect_diag.dxf` | 共享给 DXF 管线、仿真和文档示例 |
| 配方导入/导出演示文件 | `control-core` 配方域 | `examples/recipes/` | `import_template_bundle.json` 与 `recipe_export.json` 归为样例，而非主数据 |
| 仿真输入样例 | `packages/engineering-contracts` + `packages/simulation-engine` | `examples/simulation/` | `rect_diag.simulation-input.json`、`sample_trajectory.json` |
| 演示型回放输出 | `packages/simulation-engine` / `engineering-contracts` | `examples/replay-data/` | 仅保存演示性质的派生样例 |

## 4. 迁移清单

| 来源 | 目标 | 分类 | 动作 | 说明 |
|---|---|---|---|---|
| `control-core/data/recipes/recipes/*` | `data/recipes/recipes/*` | 源码资产 | 复制 | 保留 legacy 目录，根级作为公开 canonical |
| `control-core/data/recipes/versions/*` | `data/recipes/versions/*` | 运行时业务产物 | 复制 | 版本记录受业务约束，不能当作缓存删除 |
| `control-core/data/recipes/templates.json` | `data/recipes/templates/template_demo.json` | 源码资产 | 结构化拆分 | 保留 bundle 示例，不再把 flat file 当 canonical |
| `control-core/data/recipes/audit/*` | `data/recipes/audit/*` | 运行时业务产物 | 复制 | 仅作为 seed 保留，后续仍由运行时写入 |
| `control-core/src/infrastructure/resources/config/files/recipes/schemas/default.json` | `data/schemas/recipes/default.json` | 源码资产 | 复制 | 旧 `src/*` 退为 fallback |
| `packages/engineering-contracts/proto/v1/*` | `data/schemas/engineering/dxf/v1/*` | 源码资产 | 复制 | owner 保留在 `engineering-contracts`，公开路径收口到根级 |
| `packages/engineering-contracts/schemas/v1/*` | `data/schemas/engineering/dxf/v1/*` | 源码资产 | 复制 | 与 proto 共同组成工程契约 |
| `packages/engineering-contracts/fixtures/cases/rect_diag/rect_diag.pb` | `data/baselines/engineering/rect_diag.pb` | 回归基线 | 复制 | 作为工程导出二进制样本 |
| `packages/engineering-contracts/fixtures/cases/rect_diag/preview-artifact.json` | `data/baselines/engineering/rect_diag.preview-artifact.json` | 回归基线 | 复制 | 作为 preview 期望输出 |
| `integration/regression-baselines/rect_diag.simulation-baseline.json` | `data/baselines/simulation/rect_diag.simulation-baseline.json` | 回归基线 | 复制 | 从集成目录收口到根级 baseline |
| `packages/engineering-contracts/fixtures/cases/rect_diag/rect_diag.dxf` | `examples/dxf/rect_diag.dxf` | 样例 | 复制 | 作为共享 DXF 样例 |
| `packages/engineering-contracts/fixtures/cases/rect_diag/simulation-input.json` | `examples/simulation/rect_diag.simulation-input.json` | 样例 | 复制 | 与 `rect_diag.pb` 配套 |
| `packages/simulation-engine/examples/sample_trajectory.json` | `examples/simulation/sample_trajectory.json` | 样例 | 复制 | package 示例程序与工作区文档共用同一份根级样例 |
| `control-core/data/recipes/import_template_bundle.json` | `examples/recipes/import_template_bundle.json` | 样例 | 复制 | 属于演示输入，不再放在主数据目录 |
| `control-core/data/recipes/recipe_export.json` | `examples/recipes/recipe_export.json` | 样例 | 复制 | 属于导出演示结果 |

## 5. 兼容路径清单

| 旧路径 | 当前状态 | 保留原因 | 退役条件 |
|---|---|---|---|
| `control-core/data/recipes/` | bridge | `control-core` 独立运行与历史脚本仍可能依赖 | 全部运行入口都切到 workspace-root 解析 |
| `control-core/src/infrastructure/resources/config/files/recipes/schemas/` | fallback | 仍有 legacy 代码和文档引用 | schema 消费方全部改为 `data/schemas/recipes/` |
| `packages/engineering-contracts/proto/` 与 `packages/engineering-contracts/schemas/` | owner 副本 | 包内测试与 owner 工作流仍依赖 | 收口脚本建立后再决定是否单向生成 |
| `dxf-pipeline/examples/rect_diag.simulation-input.json` | bridge 样例 | 旧 README 和脚本仍引用 | 文档与脚本全部切换到根级 `examples/` |
| `packages/simulation-engine/examples/sample_trajectory.json` | bridge 样例 | 包内示例程序与测试仍沿用 | 包内示例全面切换到根级样例后 |

## 6. 已标记但暂不清理的脏数据

| 路径 | 判定 | 当前处理 |
|---|---|---|
| `dxf-pipeline/tests/fixtures/imported/uploads-dxf/live/*` | 运行时导入/上传产物，命名带时间戳，非正式样例 | 保留观察，不迁入 canonical |
| `dxf-pipeline/tests/fixtures/imported/uploads-dxf/previews/*.html` | 预览生成产物 | 保留原位，后续按需要重建 |
| `dxf-pipeline/tmp/` | 临时目录 | 不迁移 |
| `control-core/build/*`、`packages/simulation-engine/build/*` | 构建产物 | 不迁移 |
| `logs/` | 日志目录 | 不迁移 |
| `packages/simulation-engine/examples/sample_result.json` | 派生输出，体积较大 | 作为实现级示例保留，不作为手工维护 canonical |

## 7. 风险说明

1. 根级 canonical 与 package/legacy 副本同时存在，短期内有内容漂移风险；迁移期必须以根级文档为准，并在评审中检查同步。
2. `data/recipes/audit/` 属于运行时业务产物，若持续入库，后续可能带来版本噪音；建议后续引入 seed 与真实运行数据分层策略。
3. `packages/engineering-contracts` 仍保留 owner 副本，当前是“公开 canonical + owner 副本”双轨模式，后续需要自动同步或单向生成策略。
