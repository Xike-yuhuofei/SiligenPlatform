# DSP E2E Spec Formal Freeze Pack

## 1. 定位

本目录是当前工作区唯一正式冻结文档集。

- 正式架构评审、模块 owner 判定、控制语义冻结、验收基线回链统一以本目录为准。
- 仓库内不再保留任何并行正式冻结文档集。

## 2. 正式轴清单

| 轴 | 文件 | 正式用途 |
|---|---|---|
| `S01` | `dsp-e2e-spec-s01-stage-io-matrix.md` | 阶段输入/输出、成功事件、失败事件、回退入口 |
| `S02` | `dsp-e2e-spec-s02-stage-responsibility-acceptance.md` | 阶段职责、owner、验收条件、禁止短路 |
| `S03` | `dsp-e2e-spec-s03-stage-errorcode-rollback.md` | 失败分层、错误码、回退目标、人工放行边界 |
| `S04` | `dsp-e2e-spec-s04-stage-artifact-dictionary.md` | 对象字典、生命周期、唯一 owner |
| `S05` | `dsp-e2e-spec-s05-module-boundary-interface-contract.md` | `M0-M11` 模块边界、命令/事件、禁止越权 |
| `S06` | `dsp-e2e-spec-s06-repo-structure-guide.md` | 仓库结构建议、依赖方向、测试落位 |
| `S07` | `dsp-e2e-spec-s07-state-machine-command-bus.md` | 状态机、命令总线、幂等与归档 |
| `S08` | `dsp-e2e-spec-s08-system-sequence-template.md` | success/block/rollback/recovery/archive 时序模板 |
| `S09` | `dsp-e2e-spec-s09-test-matrix-acceptance-baseline.md` | 测试矩阵、验收基线、自动化优先级 |
| `S10` | `dsp-e2e-spec-s10-frozen-directory-index.md` | 索引入口、审阅顺序、统一冻结关键词 |

## 2.1 冻结补充标准

以下文件位于同一正式冻结目录内，作为 `S01-S10` 的冻结补充标准使用：

| 类型 | 文件 | 正式用途 |
|---|---|---|
| 补充标准 | `project-chain-standard-v1.md` | 冻结项目内正式对象链、live 控制链与支撑链的统一定义与分类边界 |
| 补充契约 | `internal_execution_contract_v_1.md` | 冻结内部执行契约与执行语义边界 |
| 补充标准 | `dxf-input-governance-standard-v1.md` | 冻结 DXF 输入治理、质量门禁、诊断报告与拒绝/修复边界 |

## 3. 历史迁移映射

| 历史路径或来源 | 当前正式目标 |
|---|---|
| `apps/control-cli/` | `apps/planner-cli/` |
| `apps/control-runtime/` | `apps/runtime-service/` |
| `apps/control-tcp-server/` | `apps/runtime-gateway/` |
| `packages/` | `modules/` / `shared/` / 目标应用面 |
| `integration/` | `tests/` |
| `tools/` | `scripts/` |
| `examples/` | `samples/` |

## 4. 使用规则

- 任何正式结论都直接回写本目录对应文件。
- 不再并行维护第二套冻结文档文件名、索引或映射层。
- 任何脚本、门禁、验证报告中涉及“正式冻结文档集”的引用，都必须指向本目录。
