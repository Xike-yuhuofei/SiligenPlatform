# Historical note
- 本文件记录的是 `2026-04-08` 当时的 governance snapshot，不应被直接当作当前 residue 状态表。
- 当前 canonical truth：
  - recipe family owner = `modules/recipe-lifecycle`
  - `IEventPublisherPort` owner = `shared/contracts/runtime/include/runtime/contracts/system/IEventPublisherPort.h`
- 若下文仍把 workflow recipe targets、`runtime_process_bootstrap/recipes/RecipeJsonSerializer.h` 或 workflow-owned event seam 视为当前 owner，请按历史快照理解。

# WORKFLOW_BOUNDARY_GOVERNANCE_REPORT

## WORKFLOW_BOUNDARY_FINDING_INGEST_REPORT

### Oracle candidates

| candidate | status | assessment |
|---|---|---|
| `docs/reports/workflow-boundary-findings-28.md` | missing | 用户指定路径不存在，不能作为主 Oracle。 |
| `tests/reports/module-boundary-bridges/module-boundary-bridges.md` | exists | 只含 1 条 bridge 规则结果，不能代表 workflow owner/boundary 全貌。 |
| `modules/workflow/docs/workflow-residue-ledger-2026-04-08.md` | exists | 含 `WF-R001..WF-R028` 原始 residue 与 `WF-R029..WF-R032` trace lanes，覆盖 owner、surface、CMake、tests、docs。选为主 Oracle。 |
| `modules/workflow/WORKFLOW_RESIDUE_LEDGER.md` | exists | 早期 14 条 residue 草案，内容已被新 ledger 覆盖，保留为历史候选。 |

### Oracle completeness judgement

- 原始 28 条 finding 以 `WF-R001..WF-R028` 认定。
- `WF-R029..WF-R032` 不是原始 28 条，而是扩展 trace lanes。
- Oracle 与 live 代码存在明显偏差：
  - `WF-R001`、`WF-R004`、`WF-R025` 的部分“外部 consumer 仍链接 `siligen_domain`”证据已过时。
  - `WF-R007`、`WF-R011`、`WF-R012`、`WF-R013`、`WF-R014`、`WF-R022`、`WF-R023`、`WF-R026` 的部分对象已迁出或删除。
  - `WF-R024` 在 recipe consumer 面发生回归，Oracle 的“已收口”结论不再成立。

### Original 28 ingest

| ID | original conclusion | workflow slice | current judgement | key evidence |
|---|---|---|---|---|
| `WF-R001` | root legacy provider / include-root residue | repo root -> workflow seam | Oracle partly stale; absorbed into broader root global include leak | `CMakeLists.txt`; `SILIGEN_WORKSPACE_PUBLIC_INCLUDE_DIRS`; `security_module -> siligen_workflow_runtime_consumer_public` |
| `WF-R002` | `process-core` compat target compiles recipe concrete | `domain/process-core` | live | `domain/process-core/CMakeLists.txt` |
| `WF-R003` | `motion-core` compat target compiles safety concrete | `domain/motion-core` | live | `domain/motion-core/CMakeLists.txt` |
| `WF-R004` | `siligen_domain` super aggregate anchors foreign public/concrete | `domain/CMakeLists.txt` | live inside workflow; Oracle external consumer list partly stale | `domain/CMakeLists.txt` |
| `WF-R005` | bridge-domain still compiles live payload | `domain/domain` | live | `domain/domain/CMakeLists.txt` |
| `WF-R006` | workflow owns dispensing execution concrete | `domain/domain/dispensing` | live | `domain/domain/CMakeLists.txt`; `PurgeDispenserProcess.cpp`; `SupplyStabilizationPolicy.cpp`; `ValveCoordinationService.cpp` |
| `WF-R007` | workflow owns machine concrete | `domain/domain/machine` | original concrete already removed; absorbed by header/doc seam drift | `domain/CMakeLists.txt`; `modules/runtime-execution/README.md` |
| `WF-R008` | workflow owns recipe domain concrete | `domain/domain/recipes` | live | `domain/domain/CMakeLists.txt`; `RecipeActivationService.cpp`; `RecipeValidationService.cpp` |
| `WF-R009` | workflow owns safety concrete | `domain/domain/safety` | live | `domain/domain/CMakeLists.txt`; `EmergencyStopService.cpp`; `InterlockPolicy.cpp` |
| `WF-R010` | workflow owns recipe application concrete | `application/usecases/recipes` | live | `application/usecases/recipes/CMakeLists.txt` |
| `WF-R011` | workflow owns valve concrete | `application/usecases/dispensing/valve` | original target gone; no longer applicable as written | current tree lacks valve target/source |
| `WF-R012` | workflow owns system concrete | `application/usecases/system` | original target gone; no longer applicable as written | current tree lacks `siligen_application_system` |
| `WF-R013` | workflow owns motion concrete / runtime assembly | `application/usecases/motion` | original concrete gone; absorbed into public/doc drift | current tree lacks `siligen_application_motion` |
| `WF-R014` | mixed owner hidden by `siligen_control_application` / alias | `application/CMakeLists.txt` | no longer applicable | current `application/CMakeLists.txt` no longer defines target |
| `WF-R015` | `siligen_workflow_application_headers` leaks foreign surface | `application/CMakeLists.txt` | live | `application/CMakeLists.txt` links job/dxf/runtime/motion/dispense/process owner targets |
| `WF-R016` | `siligen_workflow_domain_headers` leaks foreign include roots | `domain/CMakeLists.txt` | original include-dir leak fixed; absorbed into compat header drift | `domain/CMakeLists.txt`; `domain/include/domain/motion/**`; `domain/include/domain/dispensing/**` |
| `WF-R017` | root workflow bundles export foreign surfaces | `modules/workflow/CMakeLists.txt` | live | `siligen_workflow_recipe_*`; `siligen_workflow_runtime_consumer_public` |
| `WF-R018` | old dispensing wrapper headers | `application/include/**/usecases/dispensing` | resolved and kept resolved | deleted wrapper headers |
| `WF-R019` | workflow compat headers for runtime contracts | `application/include/workflow/application/services/**` | resolved and kept resolved | deleted compat headers |
| `WF-R020` | deprecated M8 compat headers still export semantics | `domain/include/domain/dispensing/**` | partially live in reduced set | `CMPTriggerService.h` forward to `dispense-packaging`; other Oracle-listed headers already absent |
| `WF-R021` | deprecated M7 compat headers still export semantics | `domain/include/domain/motion/**` | live | `TrajectoryPlanner.h`; `SpeedPlanner.h` |
| `WF-R022` | planning-boundary canonical header still aliases old path | `domain/include/domain/planning-boundary` | resolved | current `ISpatialIndexPort.h` is standalone |
| `WF-R023` | recovery canonical header still aliases old path | `domain/include/domain/recovery` | resolved | current `IRedundancyRepositoryPort.h` is standalone |
| `WF-R024` | app consumers cut off old workflow include roots | apps seam | regressed on recipe surface | `apps/planner-cli/CommandHandlers.Recipe.cpp`; `apps/runtime-service/container/ApplicationContainer.Recipes.cpp`; `apps/runtime-gateway/transport-gateway/include/.../tcp_facade_builder.h` |
| `WF-R025` | runtime-execution still reverse-depends on workflow compat surface | runtime-execution seam | live but shifted | `modules/runtime-execution/application/CMakeLists.txt`; `modules/runtime-execution/contracts/runtime/CMakeLists.txt`; `modules/runtime-execution/runtime/host/CMakeLists.txt` |
| `WF-R026` | workflow adapters still own DXF parsing concrete | `adapters/infrastructure/adapters/planning/dxf` | resolved | current adapters only keep redundancy adapter |
| `WF-R027` | workflow canonical tests still depend on foreign owners | `tests/canonical` | live | `tests/canonical/CMakeLists.txt` |
| `WF-R028` | README/module/CMake comment cleaner than reality | docs / metadata | partially fixed this round; still live cross-module | `modules/workflow/README.md`; `modules/workflow/module.yaml`; `modules/motion-planning/README.md`; `modules/runtime-execution/README.md`; `modules/workflow/domain/domain/README.md` |

## WORKFLOW_STRUCTURAL_GOVERNANCE_THEMES

| theme | findings | common root cause | pollution radius | single-point-fix miss | level |
|---|---|---|---|---|---|
| `compat-targets-still-own-payload` | `R002 R003 R004 R005` | 把迁移期 bridge/compat target 当长期承载面 | workflow domain build graph + root metadata | 只删某个 `.cpp` 仍保留 aggregate / compat anchoring | whole workflow boundary |
| `foreign-concrete-still-lives-in-workflow` | `R006 R008 R009 R010` | M0 没冻结终态 owner，就继续把 dispense/recipe/safety concrete 留在 workflow | `domain/domain/**`, `application/usecases/recipes/**`, apps consumers | 只改 public header 不能移除真实 owner 污染 | submodule / cross-module seam |
| `public-surface-and-bundle-drift` | `R015 R016 R017 R020 R021 R024` | 把 foreign owner contract/header/targets 打包成 workflow surface | workflow public roots, apps, runtime-execution | 只删 wrapper 不 cutover consumer，旧 surface 会回流 | cross-module seam |
| `runtime-reverse-dependency-drift` | `R025` + expansion | downstream runtime-execution 继续把 workflow 当 domain provider | runtime contracts, runtime host, runtime application | 只清 workflow 内部，runtime 会把 seam 再拉回来 | cross-module seam |
| `tests-and-gates-lie-about-boundary` | `R027` + unit-test expansion + gate false green | 把 foreign-owner 测试放在 workflow，且 bridge gate 只测已登记桥接规则 | workflow tests, validation scripts, reports | 只改 CMake 不补 guard，脏结构会回流 | whole workflow boundary |
| `docs-and-module-declarations-out-run-reality` | `R028` + doc conflicts | README/module/architecture 文档提前宣告 closeout，或跨模块互相矛盾 | workflow docs + motion-planning/runtime-execution docs | 只改 workflow README 会留下跨模块假真值 | whole workflow boundary |

## WORKFLOW_BOUNDARY_EXPANSION_LEDGER

| trigger | expanded scope | new pollution point | same root? | include this round? | evidence |
|---|---|---|---|---|---|
| `R010/R017/R024` | `apps/planner-cli`, `apps/runtime-service`, `apps/runtime-gateway` | app consumers still `#include "workflow/application/usecases/recipes/*"` and link `siligen_workflow_recipe_*` | yes | yes | `apps/planner-cli/CommandHandlers.Recipe.cpp`; `apps/runtime-service/CMakeLists.txt`; `apps/runtime-gateway/transport-gateway/CMakeLists.txt` |
| `R025` | `modules/runtime-execution/**` | runtime-execution no longer links `siligen_domain`, but still PUBLIC-links `siligen_workflow_domain_headers` and consumes `domain/supervision/ports/IEventPublisherPort.h` via runtime contracts | yes | yes | `modules/runtime-execution/application/CMakeLists.txt`; `contracts/runtime/CMakeLists.txt`; `runtime/host/CMakeLists.txt` |
| `R001/R017` | repo root | root global include dirs still pre-wire workflow + dxf + runtime public roots, and `security_module` links `siligen_workflow_runtime_consumer_public` | yes | yes | repo `CMakeLists.txt` |
| `R028` | `modules/motion-planning/README.md` | README says execution owner fixed in `modules/workflow/domain/domain/motion/domain-services/`, directly contradicting M0 baseline | yes | yes | `modules/motion-planning/README.md` |
| `R028` | `modules/runtime-execution/README.md` | README says machine execution state canonical domain model surface comes from workflow `domain/machine/aggregates/DispenserModel.h` | yes | yes | `modules/runtime-execution/README.md` |
| `R028` | `modules/workflow/domain/domain/README.md`, `application/usecases/README.md` | workflow internal docs still declare motion/machine/recipes/safety/system as workflow domain/application facts | yes | yes | those two README files |
| `R027` | `modules/workflow/tests/unit/**` | unit dir carried source-bearing integration and foreign-owner tests | yes | yes, fixed | `tests/unit/CMakeLists.txt`; deleted `MotionOwnerBehaviorTest.cpp`; moved `PlanningFailureSurfaceTest.cpp` |
| `R028` | workflow shell-only reserved directories | reserved directories had no shell-only guard and could silently re-accumulate live code | yes | yes, fixed | shell-only README guard + contract/gate guards |
| Oracle vs reality | `tests/reports/module-boundary-bridges-governance` | bridge gate passes green while owner/boundary residues remain | root cause adjacent | yes | `tests/reports/module-boundary-bridges-governance/module-boundary-bridges.md` |

## WORKFLOW_TARGET_BOUNDARY_BASELINE

1. `workflow` 终态 owner 边界只包括：
   - `WorkflowContracts`
   - planning trigger orchestration
   - phase-control / workflow execution orchestration
   - recovery governance metadata、ports、scoring 与其最小 adapter
   - workflow stage / failure / recovery 事实
2. `contracts/` 终态职责：只承载 `M0` 专属稳定契约；不得承载 runtime、motion、dispense、recipe 的 owner contract。
3. `application/` 终态职责：只保留 `planning-trigger/`、`phase-control/`、`recovery-control/`；不允许 recipe CRUD、valve、system、motion runtime assembly concrete。
4. `domain/` 终态职责：只保留 workflow 自有值对象、recovery governance ports/types、必要的 orchestration boundary ports；不允许 dispense/recipe/safety/motion execution concrete。
5. `adapters/` 终态职责：只允许 workflow owner port 的最小 concrete adapter；当前仅 `redundancy` 仍可暂留。
6. `tests/` 终态职责：`tests/canonical/` 是唯一 source-bearing 承载面；其余跨 owner smoke / 回归占位分层只承载最小证明；`unit/` 只允许空壳或说明。
7. shell-only 保留目录可以继续存在，但只能作为空目录或说明目录；任何 live 代码都不允许存在。
8. 必须删除或退出 build graph 的内容：
   - `siligen_process_core`
   - `siligen_motion_core`
   - `siligen_domain`
   - `siligen_workflow_recipe_domain_public`
   - `siligen_workflow_recipe_application_public`
   - `siligen_workflow_recipe_serialization_public`
   - `siligen_workflow_runtime_consumer_public`
   - `domain/domain/dispensing/**`、`domain/domain/recipes/**`、`domain/domain/safety/**`
   - `application/usecases/recipes/**`
9. workflow 的终态 public surface 只允许：
   - `workflow/contracts/*`
   - `application/planning-trigger/*`
   - `application/phase-control/*`
   - `application/recovery-control/*`
   - `domain/recovery/**` 等 workflow 自有 header
10. workflow 的终态 include root 规则：
    - `siligen_workflow_*_headers` 只暴露 workflow 自己的 include root。
    - 不允许通过 workflow header bundle 透传 foreign owner include/link。
11. workflow 的终态 target 暴露规则：
    - `siligen_module_workflow` 只代表 M0 orchestration surface。
    - root 级 module target 不得再透传 recipe/runtime consumer/adapters foreign surface。
12. runtime wiring / assembly 终态落点：
    - concrete runtime assembly 固定在 `modules/runtime-execution/` 或 `apps/runtime-service/`。
    - workflow 只保留 port、request/result、orchestration call site。
13. docs / metadata 终态表达：
    - workflow README/module.yaml 只能写 live 事实，不得提前宣告 bridge-only closeout。
    - 其他模块 README 不得把 foreign owner 固定回 workflow。
14. 明确不允许继续存在的类型：
    - compat target 编 real source
    - deprecated header 导出真实 foreign owner 语义
    - root bundle target 透传 foreign public surface
    - tests/canonical 之外的 source-bearing workflow tests
    - services/examples 内的 live code

## WORKFLOW_BOUNDARY_GOVERNANCE_MASTER_LEDGER

### Active / must-round items

| ID | path / target | nominal role | actual role | action | type | canonical landing | risk | this round |
|---|---|---|---|---|---|---|---|---|
| `WF-L001` | `modules/workflow/domain/process-core` / `siligen_process_core` | compat bridge | recipe concrete build root | `delete` or `forward-only` | compat shell | non-workflow recipe owner | critical | yes |
| `WF-L002` | `modules/workflow/domain/motion-core` / `siligen_motion_core` | compat bridge | safety concrete build root | `delete` or `forward-only` | compat shell | safety/runtime owner | critical | yes |
| `WF-L003` | `modules/workflow/domain/CMakeLists.txt` / `siligen_domain` | domain public aggregate | foreign super-aggregate | `delete` or `forward-only` | target exposure drift | explicit owner targets | critical | yes |
| `WF-L004` | `modules/workflow/domain/domain/CMakeLists.txt` | bridge-domain | live dispensing/recipe/safety assembly root | `split` + `move-out` | bridge residue | owner modules | critical | yes |
| `WF-L005` | `modules/workflow/application/usecases/recipes/**` / `siligen_recipe_core` | workflow app usecase | recipe CRUD / bundle concrete | `move-out` | owner mismatch | dedicated recipe owner | high | yes |
| `WF-L006` | `modules/workflow/application/CMakeLists.txt` / `siligen_workflow_application_headers` | workflow app headers | foreign include/link distributor | `retarget` | include root drift | workflow-only headers | critical | yes |
| `WF-L007` | `modules/workflow/CMakeLists.txt` / `siligen_workflow_recipe_*` | module public bundles | recipe surface bundle | `delete` | dead public surface / owner mismatch | non-workflow recipe owner | high | yes |
| `WF-L008` | `modules/workflow/CMakeLists.txt` / `siligen_workflow_runtime_consumer_public` | workflow runtime consumer surface | root mixed bundle | `delete` | target exposure drift | runtime-execution/app-local targets | high | yes |
| `WF-L009` | `modules/workflow/domain/include/domain/motion/**` | workflow domain public | M7 compat headers | `delete` | dead public surface | motion-planning | high | yes |
| `WF-L010` | `modules/workflow/domain/include/domain/dispensing/**` | workflow domain public | M8 compat headers / mixed live concrete headers | `split` + `delete` | dead public surface | dispense-packaging / runtime-execution | high | yes |
| `WF-L011` | `modules/workflow/tests/canonical/CMakeLists.txt` | canonical workflow tests | foreign-owner integration assembly | `split` | test boundary drift | owner-local tests + repo integration | high | yes |
| `WF-L012` | `apps/*` recipe consumers | downstream consumer | still bound to workflow recipe surface | `retarget` | boundary leak | future recipe owner surface | high | not yet |
| `WF-L013` | `modules/runtime-execution/**` seam | downstream runtime owner | still consumes workflow domain public/contracts seam | `retarget` | reverse dependency | runtime_execution canonical contracts | high | not yet |
| `WF-L014` | root `CMakeLists.txt` | workspace infra | global workflow/dxf/runtime surface prewire | `retarget` | include root drift | per-target dependencies | medium | not yet |
| `WF-L015` | cross-module README/module docs | documentation | conflicting owner truth | `update` | docs drift | live owner truth | medium | partial |

### This-round closed items

| ID | path / target | action | evidence |
|---|---|---|---|
| `WF-F001` | old `application/include/**/usecases/dispensing/*` wrappers | kept deleted and guarded | deleted files + bridge contract test |
| `WF-F002` | `domain/include/domain/system/ports/IEventPublisherPort.h` | kept deleted and guarded | deleted file + bridge contract test |
| `WF-F003` | `tests/unit/MotionOwnerBehaviorTest.cpp` | deleted from workflow | file removed; runtime-execution already owns equivalent test surface |
| `WF-F004` | `tests/unit/PlanningFailureSurfaceTest.cpp` | moved to integration | new `tests/integration/PlanningFailureSurfaceTest.cpp` |
| `WF-F005` | `tests/unit/CMakeLists.txt` | downgraded to registration-only | no `add_executable(` remains |
| `WF-F006` | workflow shell-only reserved directories | shell-only rule written and guarded | docs + contract test |
| `WF-F007` | workflow README/module metadata | downgraded from false-clean language to live-state language | updated `README.md`; `module.yaml` |

## WORKFLOW_BOUNDARY_GOVERNANCE_EXECUTION_PLAN

### Layer 1: Boundary freeze

| step | object | action | why first | impact | validation | rollback |
|---|---|---|---|---|---|---|
| `1.1` | Oracle + docs | freeze `WF-R001..028` as probe set and mark stale/resolved/live | prevents point fixes against stale Oracle | all later analysis | repo search + report doc | revert doc only |
| `1.2` | workflow README/module metadata | remove false-clean claims | stops docs from outrunning code | workflow readers, scripts | contract test + manual diff | doc revert |
| `1.3` | tests boundary rule | freeze `tests/canonical` as only source-bearing root | prevents test drift while deeper refactor waits | workflow tests | contract test + `rg` | revert CMake/docs |

### Layer 2: Structural cleanup

| step | object | action | why before call-site work | impact | validation | rollback |
|---|---|---|---|---|---|---|
| `2.1` | dead compat shims | delete obsolete wrapper headers and old event publisher shim | removes fake-canonical surfaces before consumer scan | workflow public headers | bridge contract + `rg` | restore files |
| `2.2` | `tests/unit` | remove foreign-owner / integration payload from unit dir | unit dir cannot keep source-bearing debt | workflow tests | contract test + file search | restore files |
| `2.3` | services/examples | enforce shell-only | blocks future garbage-dump behavior | low | contract test | revert docs/guards |

### Layer 3: Call-site and assembly recovery

| step | object | action | why after structure cleanup | impact | validation | rollback |
|---|---|---|---|---|---|---|
| `3.1` | apps recipe consumers | retarget off workflow recipe surface | producer surface must be frozen before consumer cutover | `apps/planner-cli`, `apps/runtime-service`, `apps/runtime-gateway` | targeted app builds | consumer revert |
| `3.2` | runtime-execution seam | stop consuming workflow domain public/contracts directly | downstream reverse dependency blocks true closeout | runtime-execution build graph | targeted build + contract tests | runtime-execution revert |
| `3.3` | workflow public bundles | collapse `siligen_workflow_recipe_*`, `siligen_workflow_runtime_consumer_public`, eventually `siligen_domain` | only valid after consumers cut over | root/module build graph | targeted configure/build | target revert |

### Layer 4: Validation and regression

| step | object | action | why last | impact | validation | rollback |
|---|---|---|---|---|---|---|
| `4.1` | bridge contract | keep file-level contract green | proves deletions/guards hold | low | `pytest tests/contracts/test_bridge_exit_contract.py -q` | revert tests/guards |
| `4.2` | bridge report | confirm guard scope, but do not treat green as closure | gate is narrower than owner governance | medium | `assert-module-boundary-bridges.ps1` | none |
| `4.3` | targeted build/test | after bigger owner moves, validate app/runtime seams | catches hidden include/link fallout | large | root build slices + ctest | per-slice revert |

## WORKFLOW_BOUNDARY_GOVERNANCE_FINAL_REPORT

### Original 28 status

| status | IDs |
|---|---|
| `fixed` | `WF-R018`, `WF-R019`, `WF-R022`, `WF-R023`, `WF-R026` |
| `absorbed by larger refactor / Oracle stale` | `WF-R001`, `WF-R004`, `WF-R007`, `WF-R011`, `WF-R012`, `WF-R013`, `WF-R014`, `WF-R016`, `WF-R020`, `WF-R025`, `WF-R028` |
| `no longer applicable` | `WF-R011`, `WF-R012`, `WF-R014` |
| `still open` | `WF-R002`, `WF-R003`, `WF-R005`, `WF-R006`, `WF-R008`, `WF-R009`, `WF-R010`, `WF-R015`, `WF-R017`, `WF-R021`, `WF-R024`, `WF-R027` |
| `blocked` | none in code; structurally blocked by missing external recipe owner landing are `WF-R002`, `WF-R008`, `WF-R010`, `WF-R024` |

### Expanded items status

| item | status |
|---|---|
| apps recipe consumers still on workflow surface | open |
| runtime-execution still consumes workflow seam | open |
| root global include leak / runtime consumer public link | open |
| cross-module README conflicts | open |
| workflow tests/unit source-bearing drift | fixed this round |
| services/examples shell-only guard gap | fixed this round |
| bridge gate false green vs owner reality | open |

### Directory before/after

- `tests/unit/` 从 source-bearing 目录降为 registration-only 空壳。
- `tests/integration/` 接收 `PlanningFailureSurfaceTest.cpp`，把 cross-owner 规划失败验证从 unit 迁出。
- workflow README / module metadata 从“已 closeout”口径改为“仍有 live residue”口径。
- services/examples 明确改成 shell-only 保留目录。

### Subdirectory outcome

| subdir | outcome |
|---|---|
| `contracts` | stable M0 contract surface, kept |
| `domain` | still heavily polluted by compat targets and foreign concrete |
| `application` | planning/phase-control usable; recipe family still polluted |
| `adapters` | currently reduced to redundancy adapter; DXF adapter residue cleared |
| `tests` | unit drift fixed; canonical foreign-owner dependency drift remains |
| `services` | shell-only guard added |
| `examples` | shell-only guard added |

### Public surface / include / target exposure changes this round

- 删除并守护旧 `usecases/dispensing/*` wrapper 与 `domain/system/ports/IEventPublisherPort.h` shim。
- `tests/unit` 不再暴露 build target，不再承载 source-bearing tests。
- README / module metadata 不再宣称 bridge-only closeout 已完成。

### Compat / bridge / legacy / façade cleanup this round

- deleted:
  - `modules/workflow/tests/unit/MotionOwnerBehaviorTest.cpp`
  - `modules/workflow/tests/unit/PlanningFailureSurfaceTest.cpp`
- moved:
  - `modules/workflow/tests/integration/PlanningFailureSurfaceTest.cpp`
- downgraded:
  - `modules/workflow/tests/unit/CMakeLists.txt` -> registration-only

### Owner / boundary closeout result

- 本轮没有把 28 条清零。
- 本轮完成的是：
  - Oracle 与 live 现实重新对齐
  - test boundary 的一个真实收口
  - shell-only 目录和已删除 compat shim 的 guardrail 落地
  - 文档从“假完成”回到“live residue”口径

### Tests / docs / metadata sync

- `python -m pytest tests/contracts/test_bridge_exit_contract.py -q` -> passed (`17 passed`)
- `scripts/validation/assert-module-boundary-bridges.ps1` -> passed, 但报告只证明“已登记 bridge 规则为绿”，不能证明 owner/boundary 已完成收口

### Remaining blockers

1. recipe owner 没有在 workflow 之外冻结 landing target，导致 `process-core`、`domain/domain/recipes`、`application/usecases/recipes` 与 apps consumer 无法一次性抽空。
2. `runtime-execution` 仍以 workflow domain public/contracts seam 作为直接依赖点，workflow 不能单边完成 clean-exit。
3. `siligen_domain`、`siligen_process_core`、`siligen_motion_core` 与 `siligen_workflow_recipe_*` 等 target 仍活在 build graph 中。
4. `tests/canonical` 仍链接 runtime/dxf/job-ingest/dispense foreign owner targets，canonical tests 没有收口到 workflow owner 自身。
5. `modules/motion-planning/README.md`、`modules/runtime-execution/README.md`、`modules/workflow/domain/domain/README.md` 等跨模块文档仍表达互相冲突的 owner 事实。

### Final verdict

`PARTIAL`
