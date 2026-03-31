# Implementation Plan: ARCH-203 Process Model Owner Boundary Repair

**Branch**: `refactor/process/ARCH-203-process-model-owner-repair` | **Date**: 2026-03-31 | **Spec**: `/specs/refactor/process/ARCH-203-process-model-owner-repair/spec.md`
**Input**: Feature specification from `/specs/refactor/process/ARCH-203-process-model-owner-repair/spec.md`

## Summary

本阶段先把 `2026-03-31` 的九模块评审包与复核报告纳入当前 clean worktree 的正式基线，再围绕 owner seam 设计一套可执行的整改顺序：先冻结 `workflow/runtime-execution` 的编排与执行边界及旧 build spine 退场条件，再收口 `process-planning -> coordinate-alignment -> process-path -> motion-planning -> dispense-packaging` 主链交接及其外部输入消费者，最后处理 `topology-feature`、`hmi-application`、宿主入口、测试归属和模块库存同步，并把复核报告里的可追溯性补强纳入正式 closeout。

## Technical Context

**Language/Version**: C++17、CMake 3.20+、Python 3.11、PowerShell 7、Markdown  
**Primary Dependencies**: GoogleTest、pytest、PyQt5/PyQtWebEngine、protobuf、spdlog、仓库根级验证脚本  
**Storage**: N/A（仅 Git 跟踪仓库文件与 `tests/reports/` 报告目录）  
**Testing**: `.\build.ps1`、`.\test.ps1`、`.\ci.ps1`、`scripts/validation/assert-module-boundary-bridges.ps1`、模块单测/契约测试/协议兼容测试  
**Target Platform**: Windows 开发环境与 CI；无机台、以 mock/静态验证为主  
**Project Type**: 多模块桌面/运行时工程的架构边界修复与治理资产规划  
**Performance Goals**: 保持现有外部行为与现有验证口径，不因 owner 收口引入新的隐式 fallback、额外对外交互步骤或明显验证退化  
**Constraints**:
- 实施前必须先把 `2026-03-31` 的 9 份评审与 1 份复核报告纳入当前 worktree，否则实现证据无法在 clean worktree 内闭环
- 所有新旧边界说明都必须使用 canonical workspace roots，不得新增 legacy 默认路径
- 兼容壳可以短期保留，但必须显式、限时、可验证退出
- `job-ingest`、`dxf-geometry`、`planner-cli`、`runtime-service`、`runtime-gateway`、`hmi-app` 都必须作为真实消费者纳入接入规则，不能只在模块侧抽象声明
- `siligen_process_runtime_core`、`siligen_domain`、`PROCESS_RUNTIME_CORE_*`、`siligen_process_runtime_core_boost_headers`、`process_runtime_core_add_test_with_runtime_environment` 这类旧 build spine/helper target 的退场或限用条件必须显式写入任务与证据
- `workflow/tests`、app 侧 owner 单测、compat re-export、未编译 stub/source、模块库存文档都必须有去留决策，不能被统称为“后续同步”
- 不得新增新的双向模块依赖、宿主 concrete 反向依赖或 helper target 长期公开面  
**Scale/Scope**: 9 个主链模块、6 类主要消费者/宿主入口、`job-ingest/dxf-geometry` 两类外部输入消费者、根级验证入口、`docs/process-model/reviews` 与 `modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md` 等治理资产

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] Branch name is compliant with `<type>/<scope>/<ticket>-<short-desc>`
- [x] Planned paths use canonical workspace roots; no new legacy default paths
- [x] Build/test validation approach is explicit via root entry points (`.\build.ps1`, `.\test.ps1`, `.\ci.ps1`) or justified subset
- [x] Compatibility/fallback behavior is explicit and time-bounded if legacy paths are involved

## Project Structure

### Documentation (this feature)

```text
specs/refactor/process/ARCH-203-process-model-owner-repair/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── README.md
│   ├── module-owner-boundary-contract.md
│   ├── consumer-access-contract.md
│   └── remediation-wave-contract.md
└── tasks.md
```

### Source Code (repository root)

```text
docs/
└── process-model/reviews/                 # 2026-03-31 评审包与复核报告的 canonical 落点

modules/
├── workflow/
├── topology-feature/
├── process-planning/
├── coordinate-alignment/
├── process-path/
├── motion-planning/
├── dispense-packaging/
├── runtime-execution/
└── hmi-application/

apps/
├── hmi-app/
├── planner-cli/
├── runtime-gateway/
└── runtime-service/

modules/
├── job-ingest/                        # M4 concrete 逆向消费者
└── dxf-geometry/                      # M6 输入适配重复实现与 M4 concrete 逆向消费者

shared/
└── contracts/

scripts/
└── validation/

tests/
└── reports/
```

**Structure Decision**: 本特性不是单目录重构，而是跨 canonical `modules/`、`apps/`、`docs/`、`scripts/validation/`、`shared/contracts/` 的边界修复规划。实现必须始终以模块 owner 为中心，宿主和脚本只承担消费、装配或验证责任。

## Design Decisions

1. **先导入评审基线，再开始实现波次**
   clean worktree 当前不包含 `2026-03-31` 评审包，因为这些文档在原工作区是未跟踪文件。`Wave 0` 必须先把这些文档纳入当前分支，使后续任务、实现和验收都能在同一 worktree 内闭环。

2. **以 owner seam 划分波次，而不是按目录数量平均拆分**
   优先级按风险和依赖方向排序：`workflow <-> runtime-execution` 与旧 build spine 为第一优先级，随后处理 `M4-M8` 主链交接及 `job-ingest/dxf-geometry` 这类外部 concrete 消费者，最后修补 `M3/M11`、宿主消费者、测试归属和模块库存。这样可以先切断最危险的双向耦合，再消除主链重复 owner 和输入边界外溢。

3. **兼容壳只允许作为短期过渡，不允许继续吸收 live 逻辑**
   所有 legacy header、compat facade、shadow adapter、重复 contract tree 都只能被定义为 `只读兼容壳`、`临时 bridge` 或 `移除目标`。一旦继续在这些位置增加逻辑，本特性即宣告失败。

4. **消费者一律改接 canonical public surface**
   `apps/hmi-app`、`apps/planner-cli`、`apps/runtime-gateway`、`apps/runtime-service`、`workflow` 编排层、`job-ingest`、`dxf-geometry` 及验证脚本，只能依赖声明的 contracts/facade/application surface；不再把模块内部路径、helper target 或宿主 concrete 逻辑当作稳定接口。

5. **复核报告中的纠偏结论优先于原评审表述**
   尤其是 `dispense-packaging` 对 `M9` 依赖归因的纠正，必须进入所有后续计划、任务和 evidence。任何仍按旧错误结论推进的整改都应被视为计划缺陷。

6. **最终验收必须回到根级入口**
   阶段内可使用 `assert-module-boundary-bridges.ps1`、定向模块测试和协议兼容测试做快反馈，但进入收尾前必须回归 `.\build.ps1`、`.\test.ps1`、`.\ci.ps1` 或其明确说明的受限子集。

7. **复核报告里的可追溯性问题属于交付范围，而不是可选优化**
   `coordinate-alignment`、`dispense-packaging`、`topology-feature`、`process-planning` 评审中缺失的哈希、diff 摘录、复核命令或精确索引，必须在 closeout 前补齐到正文或附录；否则这些评审不能作为“已闭环证据”使用。

## Planned Artifacts

- `research.md`: 收敛 baseline、波次策略、兼容壳策略、消费者接入规则和验证策略
- `data-model.md`: 建模模块 charter、owner artifact、消费者规则、兼容壳、影子实现、波次与 evidence
- `contracts/module-owner-boundary-contract.md`: 定义 9 个模块的 owner/forbidden boundary
- `contracts/consumer-access-contract.md`: 定义宿主与消费者的允许接入面
- `contracts/remediation-wave-contract.md`: 定义 Wave 0-4 的入场条件、退出条件和阻断项
- `quickstart.md`: 给实施者的基线确认、波次顺序与验证命令
- `docs/process-model/reviews/*20260331*.md`：作为 implementation 前必须导入的 review baseline，并在 closeout 前补齐复核报告要求的可追溯性证据

## Validation Strategy

### Required Gates

1. Baseline completeness check
   - 当前 worktree 中必须存在 9 份 `2026-03-31` 模块评审文档与 1 份复核报告
   - 缺失任一文件时，只能继续做 planning，不得进入 implementation

2. Boundary guardrail
   - `pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/validation/assert-module-boundary-bridges.ps1 -WorkspaceRoot . -ReportDir tests/reports/module-boundary-bridges`

3. Root build/test/ci entry points
   - `pwsh -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile CI -Suite contracts`
   - `pwsh -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite contracts -ReportDir tests/reports/arch-203 -FailOnKnownFailure`
   - `pwsh -NoProfile -ExecutionPolicy Bypass -File .\ci.ps1 -Suite contracts -ReportDir tests/reports/ci-arch-203`

4. Wave-specific targeted validation
   - `runtime-execution` / `workflow`：host、application、compatibility 相关单测、旧 build spine/helper target 引用检查、契约测试
   - `process-planning` 至 `dispense-packaging`：规划链、协议兼容、bridge 规则测试，以及 `job-ingest/dxf-geometry` 的 forbidden concrete dependency 检查
   - `hmi-application`：`pytest` 单测、app compat re-export 清理检查、gateway/hmi formal contract guard
   - `docs/process-model/reviews`：复核报告要求的哈希/命令/精确索引补档检查

### Validation Notes

- 在 planning 阶段只冻结验证策略，不宣称任何实现已通过。
- 若根级入口因独立已知仓库状态失败，必须在 evidence 中写明失败层、失败原因、责任模块和后续 owner，不能用定向测试通过替代。

## Post-Design Constitution Check

- [x] 设计产物全部落在 `specs/`，实施目标全部指向 canonical roots
- [x] 根级验证入口与受限子集都已显式写明
- [x] 兼容壳被定义为显式、限时、可退出的过渡结构
- [x] baseline review pack 已作为当前分支 implementation 输入落位到 `docs/process-model/reviews/`

## Known Blockers

- 根级 `.\build.ps1`、`.\test.ps1` 与 `scripts/validation/run-local-validation-gate.ps1` 当前都被同一环境前置条件阻断：`scripts/bootstrap/bootstrap-third-party.ps1` 未解析到 `boost` bundle，且本地未配置 `SILIGEN_THIRD_PARTY_ARTIFACT_ROOT` / `SILIGEN_THIRD_PARTY_BASE_URI`。
- `ci.ps1` 的 HMI formal launcher contract 前置条件已恢复，但其后续执行仍在同一 `boost` bootstrap 阻断处失败；closeout 证据必须继续以根级阻断报告方式记录，而不能伪报通过。
- `run-local-validation-gate.ps1` 已新增 review baseline / supplement gate，但根级入口在本地仍会先被 `boost` bootstrap 阻断，因此该 gate 只完成了脚本落位，尚未纳入完整 root-entry 通过证据。
