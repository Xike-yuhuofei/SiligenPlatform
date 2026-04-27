# DSP E2E Spec Formal Freeze Pack

Status: Frozen Index
Authority: Directory Entry Point
Applies To: `docs/architecture/dsp-e2e-spec/`
Owns / Covers: Codex reading route, authority routing, conflict priority, formal axis list
Must Not Override: `S01-S10` formal spec axes and supplement standards
Read When: choosing the authoritative document before implementation, review, or boundary judgment
Conflict Priority: route to the owning spec; this README is navigation only
Codex Keywords: read first, authority routing, conflict priority, S01-S10, supplement standard

---

## Codex Read First

本目录采用单一正式冻结文档集：`S01-S10` 是正式规格轴，补充标准只能细化对应领域，不得覆盖正式规格轴。

| 任务 / 问题 | 优先读取 | 辅助读取 | 裁决口径 |
|---|---|---|---|
| owner / 模块边界 / 越权判断 | `dsp-e2e-spec-s05-module-boundary-interface-contract.md` | `dsp-e2e-spec-s04-stage-artifact-dictionary.md`, `dsp-e2e-spec-s02-stage-responsibility-acceptance.md`, `project-chain-standard-v1.md` | 模块 owner 与越权边界以 S05 为准 |
| 状态机 / 命令 / 事件闭环 | `dsp-e2e-spec-s07-state-machine-command-bus.md` | `dsp-e2e-spec-s08-system-sequence-template.md`, `dsp-e2e-spec-s03-stage-errorcode-rollback.md` | 状态、命令、事件闭环以 S07/S08 为准 |
| 测试 / 验收 / 回归基线 | `dsp-e2e-spec-s09-test-matrix-acceptance-baseline.md` | `dsp-e2e-spec-s02-stage-responsibility-acceptance.md`, `dsp-e2e-spec-s03-stage-errorcode-rollback.md` | 测试与验收基线以 S09 为准 |
| DXF 输入治理 / 质量门禁 / 诊断报告 | `dxf-input-governance-standard-v1.md` | `dsp-e2e-spec-s01-stage-io-matrix.md`, `dsp-e2e-spec-s02-stage-responsibility-acceptance.md`, `dsp-e2e-spec-s03-stage-errorcode-rollback.md`, `dsp-e2e-spec-s04-stage-artifact-dictionary.md`, `dsp-e2e-spec-s05-module-boundary-interface-contract.md`, `dsp-e2e-spec-s09-test-matrix-acceptance-baseline.md` | DXF 补充标准只细化输入治理，不覆盖 S01-S10 |
| live 链与正式对象链区分 | `project-chain-standard-v1.md` | `dsp-e2e-spec-s01-stage-io-matrix.md`, `dsp-e2e-spec-s05-module-boundary-interface-contract.md`, `dsp-e2e-spec-s07-state-machine-command-bus.md` | live 链不是正式对象链事实源 |

## Conflict Priority

- 对象字段、引用、生命周期以 `S04` 为准。
- 模块 owner、接口边界、禁止越权以 `S05` 为准。
- 阶段职责、验收条件、禁止短路以 `S02` 为准。
- 状态、命令、事件闭环以 `S07/S08` 为准。
- 测试矩阵、验收基线、回归证据以 `S09` 为准。
- `project-chain-standard-v1.md`、`internal_execution_contract_v_1.md`、`dxf-input-governance-standard-v1.md` 只作为冻结补充标准；若与 `S01-S10` 冲突，回到对应正式规格轴裁决。

---

## 1. 定位

本目录是当前工作区唯一正式冻结文档集。

- 正式架构评审、模块 owner 判定、控制语义冻结、验收基线回链统一以本目录为准。
- 仓库内不维护并行正式冻结入口、影子索引或替代事实源。

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
| 补充标准 | `dxf-input-governance-standard-v1.md` | 冻结 DXF 输入治理、验证报告、错误码、policy、fixture 与门禁收敛标准 |

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
- 不并行维护替代冻结文档文件名、索引或映射层。
- 任何脚本、门禁、验证报告中涉及“正式冻结文档集”的引用，都必须指向本目录。
