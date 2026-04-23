# 1. Input baseline
- 唯一输入基线：`D:\Projects\SiligenSuite\modules\workflow\docs\workflow-residue-ledger-2026-04-08.md`
- 本轮不补审计、不改代码、不改构建；只对 ledger 做 normalization 与 batched cleanup planning。
- 冻结口径沿用 ledger 与阶段约束：
  - `modules/workflow` 只拥有 `M0 workflow` 编排、阶段推进、规划链触发边界、流程状态与调度决策事实。
  - `src/`、`process-runtime-core/`、compat/bridge 壳层只能是 `shell-only` / `forward-only` / `deprecated compatibility`。
  - clean-exit 目标是退出 foreign concrete，不是保留长期迁移兼容。
- 基线 residue 数：
  - `confirmed`: `WF-R001` ~ `WF-R028`
  - `suspected`: `WF-R029` ~ `WF-R032`
- normalization 后，当前 ledger 不是“32 个彼此独立的问题”，而是 7 条主依赖链上的 32 个观测点。

# 2. Ledger normalization
## 2.1 Deduplicated residues
- 没有“完全重复”的 residue，但有 7 组高重叠链，必须归并看待，不能按 32 个互不相关条目排批。
- `BG-legacy-provider` 链：`WF-R001`、`WF-R004`、`WF-R025`、`WF-R032`
  - 同一 build graph 问题的不同表现：root 仍要求 legacy provider，workflow 仍产出 super-aggregate，下游 `runtime-execution` 仍消费它，gate 还看不见 target-level residue。
- `Bridge-domain-live-payload` 链：`WF-R005`、`WF-R006`、`WF-R007`、`WF-R008`、`WF-R009`
  - `WF-R005` 是父问题；`WF-R006~R009` 是其在 dispensing/machine/recipe/safety 上的 payload 展开。
- `Workflow/application public-surface` 链：`WF-R018`、`WF-R019`、`WF-R015`、`WF-R024`、`WF-R017`、`WF-R027`
  - 同一 public surface 问题从“内部 canonical 头仍指向旧路径”一路外溢到“apps 还在吃旧 include 根”“module root 仍打 bundle”“tests 仍依赖 foreign owner”。
- `Workflow/domain public-surface` 链：`WF-R020`、`WF-R021`、`WF-R022`、`WF-R023`、`WF-R016`、`WF-R025`、`WF-R030`
  - 同一 domain surface 问题从 deprecated header/canonical alias，一直延伸到 `runtime-execution` 反向依赖和 suspected old consumer。
- `Workflow/application mixed-owner` 链：`WF-R010`、`WF-R011`、`WF-R012`、`WF-R013`、`WF-R014`
  - `WF-R014` 不是独立 root cause，而是前四个 concrete owner-drift 被单个 target 名字掩盖后的症状。
- `False-clean governance` 链：`WF-R028`、`WF-R029`、`WF-R032`
  - 文档、validator、exit gate 都在制造“目录已干净”的假完成空间。
- 需要先拆粗粒度 residue 再排批：
  - `WF-R024` 过粗，至少应拆成 `motion/system/dispensing/recipe/DXF` 消费者切片；不能把它当一个可直接执行 batch。
  - `WF-R025` 过粗，至少应拆成 `link unlink` 与 `runtime contracts include retarget` 两个执行切片；本计划里先保留一个 batch 壳，但标记 blocked。
- `suggested_disposition` 过于模糊、需先细化的 residue：
  - 目的地 owner 未冻结：`WF-R002`、`WF-R003`、`WF-R006`、`WF-R008`、`WF-R010`、`WF-R011`、`WF-R012`、`WF-R026`
  - split 轴未冻结：`WF-R004`、`WF-R007`、`WF-R009`、`WF-R013`、`WF-R014`、`WF-R017`、`WF-R027`
  - 仍只是 trace 占位符：`WF-R028`、`WF-R029`、`WF-R030`、`WF-R031`、`WF-R032`
- 仍为 `suspected`、不能直接进入执行批次的 residue：
  - `WF-R029`、`WF-R030`、`WF-R031`、`WF-R032`

## 2.2 Root causes vs symptoms
- `root`：
  - `WF-R001`、`WF-R002`、`WF-R003`、`WF-R004`、`WF-R005`
  - `WF-R010`、`WF-R011`、`WF-R012`、`WF-R013`
  - `WF-R015`、`WF-R016`
  - `WF-R026`、`WF-R031`
- `mixed`：
  - `WF-R006`、`WF-R007`、`WF-R008`、`WF-R009`
  - `WF-R017`、`WF-R025`
  - `WF-R029`、`WF-R032`
- `symptom`：
  - `WF-R014`
  - `WF-R018`、`WF-R019`、`WF-R020`、`WF-R021`、`WF-R022`、`WF-R023`
  - `WF-R024`、`WF-R027`、`WF-R028`、`WF-R030`
- 关键 root/symptom 对应关系：
  - `WF-R014` 是 `WF-R010~WF-R013` 的命名症状，不应先做。
  - `WF-R027` 是 `WF-R015/016/017/024/025` 的测试层症状，不应先做。
  - `WF-R028` 是 `WF-R001/004/015/016/017` 的叙事层症状，不应先做。
  - `WF-R030` 是 `WF-R022/023` 向 repo 级 include graph 外溢后的 suspected 症状。
  - `WF-R005` 是 bridge-domain 根因；`WF-R006~R009` 是其 owner-family 化后的展开。

## 2.3 Execution readiness
| residue_id | normalized_family | root_or_symptom | execution_readiness | depends_on_residue_ids | can_be_closed_independently | blast_radius |
|---|---|---|---|---|---|---|
| `WF-R001` | `bg-root-provider` | `root` | `ready` | `-` | `yes` | `large` |
| `WF-R002` | `recipe-compat-target` | `root` | `blocked` | `WF-R008, WF-R010` | `no` | `medium` |
| `WF-R003` | `safety-compat-target` | `root` | `blocked` | `WF-R009, WF-R025` | `no` | `medium` |
| `WF-R004` | `domain-super-aggregate` | `root` | `blocked` | `WF-R001, WF-R002, WF-R003, WF-R025` | `no` | `large` |
| `WF-R005` | `bridge-domain-root` | `root` | `blocked` | `WF-R006, WF-R007, WF-R008, WF-R009` | `no` | `large` |
| `WF-R006` | `dispense-domain-concrete` | `mixed` | `blocked` | `WF-R020` | `yes` | `medium` |
| `WF-R007` | `machine-calibration-mixed` | `mixed` | `needs-trace` | `-` | `no` | `medium` |
| `WF-R008` | `recipe-domain-concrete` | `mixed` | `blocked` | `WF-R010` | `yes` | `medium` |
| `WF-R009` | `safety-domain-concrete` | `mixed` | `blocked` | `WF-R003, WF-R021, WF-R025` | `no` | `large` |
| `WF-R010` | `recipe-app-concrete` | `root` | `blocked` | `-` | `yes` | `medium` |
| `WF-R011` | `valve-app-concrete` | `root` | `needs-trace` | `WF-R020` | `yes` | `medium` |
| `WF-R012` | `system-app-concrete` | `root` | `needs-trace` | `WF-R019` | `yes` | `medium` |
| `WF-R013` | `motion-app-concrete` | `root` | `blocked` | `WF-R021, WF-R025` | `no` | `large` |
| `WF-R014` | `application-mixed-naming` | `symptom` | `blocked` | `WF-R010, WF-R011, WF-R012, WF-R013` | `no` | `medium` |
| `WF-R015` | `application-header-bundle` | `root` | `blocked` | `WF-R018, WF-R019, WF-R024` | `no` | `large` |
| `WF-R016` | `domain-header-bundle` | `root` | `blocked` | `WF-R020, WF-R021, WF-R022, WF-R023, WF-R025` | `no` | `large` |
| `WF-R017` | `module-root-public-bundle` | `mixed` | `blocked` | `WF-R015, WF-R016` | `no` | `large` |
| `WF-R018` | `planning-phase-header-alias` | `symptom` | `ready` | `-` | `yes` | `small` |
| `WF-R019` | `runtime-service-compat-header` | `symptom` | `needs-trace` | `WF-R024` | `yes` | `medium` |
| `WF-R020` | `dispense-compat-header` | `symptom` | `needs-trace` | `WF-R024` | `yes` | `medium` |
| `WF-R021` | `motion-compat-header` | `symptom` | `blocked` | `WF-R024, WF-R025` | `no` | `large` |
| `WF-R022` | `planning-boundary-alias` | `symptom` | `ready` | `-` | `yes` | `small` |
| `WF-R023` | `recovery-alias` | `symptom` | `ready` | `-` | `yes` | `small` |
| `WF-R024` | `external-old-usecase-consumers` | `symptom` | `blocked` | `WF-R018, WF-R019, WF-R020, WF-R021` | `no` | `large` |
| `WF-R025` | `runtime-execution-reverse-dep` | `mixed` | `blocked` | `WF-R020, WF-R021, WF-R022, WF-R023` | `no` | `large` |
| `WF-R026` | `dxf-adapter-concrete` | `root` | `needs-trace` | `-` | `yes` | `medium` |
| `WF-R027` | `workflow-tests-foreign-deps` | `symptom` | `blocked` | `WF-R015, WF-R016, WF-R017, WF-R024, WF-R025` | `yes` | `medium` |
| `WF-R028` | `doc-false-clean` | `symptom` | `blocked` | `WF-R001, WF-R004, WF-R015, WF-R016, WF-R017` | `no` | `small` |
| `WF-R029` | `validator-foreign-wiring` | `mixed` | `needs-trace` | `WF-R015, WF-R017` | `no` | `medium` |
| `WF-R030` | `legacy-system-planning-consumers` | `symptom` | `needs-trace` | `WF-R022, WF-R023` | `no` | `medium` |
| `WF-R031` | `offline-engineering-tools` | `root` | `needs-trace` | `-` | `no` | `medium` |
| `WF-R032` | `exit-gate-coverage-gap` | `mixed` | `needs-trace` | `WF-R001, WF-R004, WF-R015, WF-R017` | `no` | `medium` |

## 2.4 Dependency chains
- `WF-R001 -> WF-R025 -> WF-R004 -> WF-R032 -> WF-R028`
  - 先断 root legacy provider 假设，再切 `runtime-execution` 反向依赖，最后才可能关闭 `siligen_domain` 与 gate/doc 假完成。
- `WF-R018 + WF-R019 -> WF-R024 -> WF-R015 -> WF-R017 -> WF-R027`
  - 先把 application canonical/compat 头修正，再切 apps consumer，再收缩 application header bundle，最后才轮到 module root bundle 和 tests。
- `WF-R020 + WF-R021 + WF-R022 + WF-R023 -> WF-R025 -> WF-R016 -> WF-R017 -> WF-R027`
  - 先把 domain public header 收口，再切 `runtime-execution` contracts/link 反向依赖，再收 domain header bundle/module root bundle/tests。
- `WF-R010 -> WF-R008 -> WF-R002`
  - recipe application/domain concrete 退出先于 `siligen_process_core` compat target 退出；否则只是换壳不换 owner。
- `WF-R020 -> WF-R006/WF-R011`
  - dispense public surface 先 canonical 化，再动 dispensing concrete；否则只会把旧 surface 留在 workflow。
- `WF-R021 -> WF-R009/WF-R013 -> WF-R003`
  - motion/safety public surface 先收口，再动 workflow 内 safety/motion concrete，最后 compat target 才能退出。
- `WF-R006 + WF-R007 + WF-R008 + WF-R009 -> WF-R005`
  - `domain/domain` 根不能先 collapse；先清 child payload family。
- `WF-R015 + WF-R016 + WF-R024 + WF-R025 -> WF-R027`
  - tests 永远是后手验证批，不是前手清理批。
- `WF-R015 + WF-R017 -> WF-R029`
  - validator 对 foreign wiring 的假设，必须在 application/root bundle 收缩之后再改。
- `WF-R001 + WF-R004 + WF-R015 + WF-R017 -> WF-R032`
  - exit gate 只能在 target-level residue 收口后再补规则，否则会误把旧结构写成新真值。

# 3. Batching rules
- 批次单位必须是“一个主 residue family + 一个明确 owner/consumer slice”；禁止以“顺手一起改”为由把多 family 合批。
- 先 `build graph / active public surface`，后 `code shell / concrete payload`，最后 `tests / docs / naming / gate narrative`。
- 任何 `bundle cleanup` 批次必须晚于其上游 `canonical header cleanup` 和下游 `consumer retarget` 批次。
- 任何 `compat-target` / `bridge-root` collapse 批次，必须晚于对应 concrete family 退出批次。
- `WF-R024`、`WF-R025` 这类粗粒度 residue 只能以 `partial-family-close` 或“先拆再做”的方式处理，不能直接作为一个大批次执行。
- 目的地 owner、target landing、protobuf/generator 边界、app-local vs module-owner 落点未冻结时，批次只能标记 `blocked` 或 `needs-trace`。
- 批次验收必须是机械判定：
  - target 不再存在/不再编译 real source
  - header 不再包含旧路径
  - consumer 不再链接旧 target
  - tests/gate 报告能直接判绿/判红
- 每批只允许一种主动作链；如果同时需要“producer cleanup + consumer retarget + naming cleanup”，则必须拆批。
- `family-close` 只代表该 residue family 关闭，不代表 `modules/workflow` 已 clean-exit。
- 任何批次若发现 scope 外 residue，只能登记并回到 ledger/batch plan，不得顺手扩散。

# 4. Batched cleanup plan
## 4.1 P0 batches
| Batch | Family / Targets / Excluded | Why This Batch First | Scope / Non-scope | Preconditions | Actions / Artifacts | Acceptance / Validation | Risk / Type / Next |
|---|---|---|---|---|---|---|---|
| `WF-B01` Root Legacy Provider Detach | family `bg-root-provider`；targets `WF-R001`；excluded `WF-R004, WF-R025, WF-R032` | 最小且机械可判定的 P0 build-graph 根切口；先消掉 root 对 workflow legacy provider 的假设 | scope：repo root `CMakeLists.txt` 的 global include root 与 provider assert；non-scope：不动 workflow 内部 target、不动 `runtime-execution` consumer | 无 | actions：`remove-from-build`, `retarget`, `add-temporary-guardrail`；artifacts：root `CMakeLists.txt` | accept：root 不再引用 `modules/workflow/domain/process-core/include`、`modules/workflow/domain/motion-core/include`、`siligen_domain` provider assert；validate：targeted configure/build | rollback `medium`；blast `large`；type `prerequisite-batch`；next green `WF-B04`；next blocked `-` |
| `WF-B02` Runtime-Execution Compat Surface Unlink | family `runtime-execution-reverse-dep`；targets `WF-R025`；excluded `WF-R004, WF-R016, WF-R021, WF-R024` | `runtime-execution` 反向锁住 workflow clean-exit；不切它，内部再干净也是假完成 | scope：`modules/runtime-execution/application/**`、`runtime/host/**` link 依赖与 `contracts/runtime/**` 头依赖；non-scope：不收 workflow domain bundle、不动 workflow concrete families | `WF-B01`, `WF-B04` green | actions：`retarget`, `remove-from-build`；artifacts：runtime-execution CMake、contracts 头、host/tests | accept：`runtime-execution` 不再链接 `siligen_domain`/`siligen_motion_core`，contracts 不再包含 workflow `domain/*`；validate：targeted build、include path check、contract tests | rollback `large`；blast `large`；type `family-close`；next green `WF-B08`；next blocked `WF-B04` |
| `WF-B03` Workflow Application Public Surface Cleanup | family `workflow-application-public-surface`；targets `WF-R018, WF-R019, WF-R015`；excluded `WF-R017, WF-R024, WF-R010, WF-R011, WF-R012, WF-R013` | application public surface 必须先 canonical，后面 app consumer / root bundle 才能切 | scope：`modules/workflow/application/include/**` 与 `application/CMakeLists.txt` public export；non-scope：不改 application concrete，不改 apps consumer | canonical owner headers 已冻结 | actions：`retarget`, `split-mixed-surface`, `delete-deprecated-surface`；artifacts：workflow application headers、interface target export | accept：`planning-trigger`/`phase-control` 头不再 forward `usecases/dispensing/*`，workflow runtime compat 头不再导出 runtime 服务语义，`siligen_workflow_application_headers` 不再 re-export foreign surface；validate：include path check、targeted build | rollback `large`；blast `large`；type `family-close`；next green `WF-B17`；next blocked `WF-B17` |
| `WF-B04` Workflow Domain Public Surface Cleanup | family `workflow-domain-public-surface`；targets `WF-R020, WF-R021, WF-R022, WF-R023, WF-R016`；excluded `WF-R025, WF-R005, WF-R006, WF-R009` | domain public surface 是 `runtime-execution` unlink 和 compat-target 退出的前置 | scope：`modules/workflow/domain/include/**` 与 `domain/CMakeLists.txt` header export；non-scope：不动 `domain/domain/**` concrete，不 collapse `siligen_domain` | canonical motion/dispense/planning-boundary/recovery surfaces 已冻结 | actions：`retarget`, `delete-deprecated-surface`, `split-mixed-surface`；artifacts：workflow domain headers、domain header bundles | accept：workflow domain public headers 不再 export M7/M8 compat 语义或 alias 旧 `domain/planning/*` / `domain/system/*`，`siligen_workflow_domain_headers` 不再暴露 motion-planning / process-core / motion-core roots；validate：include path check、targeted build | rollback `large`；blast `large`；type `family-close`；next green `WF-B02`；next blocked `WF-B19` |
| `WF-B05` Workflow Module-Root Public Bundle Cleanup | family `module-root-public-bundle`；targets `WF-R017`；excluded `WF-R015, WF-R016, WF-R024, WF-R025` | root bundle 只能在 application/domain 子 surface 清洁后再收；否则只是在顶层继续藏污 | scope：`modules/workflow/CMakeLists.txt` interface bundles；non-scope：不动 owner concrete、compat target | `WF-B03`, `WF-B04` green | actions：`split-mixed-surface`, `delete-deprecated-surface`, `retarget`；artifacts：workflow root CMake、`siligen_module_workflow`、`siligen_workflow_*_public` | accept：`siligen_module_workflow` 与 `siligen_workflow_*_public` 不再静默透传 recipe/runtime consumer/adapter foreign surfaces；validate：targeted configure/build | rollback `medium`；blast `large`；type `family-close`；next green `WF-B20`；next blocked `WF-B03` |
| `WF-B06` Recipe Compat Target Exit | family `recipe-compat-target`；targets `WF-R002`；excluded `WF-R008, WF-R010, WF-R017` | active compat target 编 real source，属于 P0 clean-exit blocker | scope：`modules/workflow/domain/process-core/**` 与 `siligen_process_core`；non-scope：不在本批内处理 recipe app/domain concrete 或 apps consumers | recipe canonical landing target 已冻结；`WF-B09` green 或同等 owner landing 已存在 | actions：`move-concrete-out`, `convert-to-forward-only`, `remove-from-build`；artifacts：process-core CMake、recipe service ownership、dependent links | accept：`siligen_process_core` 不再编译 real `.cpp`，`process-core` 仅 forward-only 或退出 build；validate：target graph check、targeted build、include path check | rollback `medium`；blast `medium`；type `family-close`；next green `WF-B08`；next blocked `WF-B09` |
| `WF-B07` Safety Compat Target Exit | family `safety-compat-target`；targets `WF-R003`；excluded `WF-R009, WF-R025, WF-R004` | active compat target 仍托管 safety payload，不先退场就无法关闭 domain aggregate | scope：`modules/workflow/domain/motion-core/**` 与 `siligen_motion_core`；non-scope：不在本批中移动 workflow safety concrete、不改 downstream consumer | safety landing owner 已冻结；`WF-B11`、`WF-B02` 路径已清 | actions：`move-concrete-out`, `convert-to-forward-only`, `remove-from-build`；artifacts：motion-core CMake、安全依赖关系 | accept：`siligen_motion_core` 不再编译 real `.cpp`，`motion-core` 仅 forward-only 或退出 build；validate：target graph check、targeted build | rollback `medium`；blast `medium`；type `family-close`；next green `WF-B08`；next blocked `WF-B11` |
| `WF-B08` Domain Super-Aggregate Collapse | family `domain-super-aggregate`；targets `WF-R004`；excluded `WF-R001, WF-R002, WF-R003, WF-R025` | `siligen_domain` 是当前 workflow 假 owner 的 build-graph 锚点 | scope：`modules/workflow/domain/CMakeLists.txt` 中 `siligen_domain` 的 export/aggregation；non-scope：不在本批中移动 child concrete families | `WF-B01`, `WF-B02`, `WF-B06`, `WF-B07`, `WF-B16` green | actions：`split-mixed-surface`, `convert-to-forward-only`, `delete-deprecated-surface`, `remove-from-build`；artifacts：workflow domain CMake、dependent links | accept：无外部模块再链接 `siligen_domain`；若保留，仅 forward-only 且不聚合 foreign concrete/public；validate：target graph check、runtime-execution targeted build、legacy-exit build slice | rollback `large`；blast `large`；type `family-close`；next green `WF-B20`；next blocked `WF-B02` |

## 4.2 P1 batches
| Batch | Family / Targets / Excluded | Why This Batch First | Scope / Non-scope | Preconditions | Actions / Artifacts | Acceptance / Validation | Risk / Type / Next |
|---|---|---|---|---|---|---|---|
| `WF-B09` Recipe Concrete Exit From Workflow | family `recipe-concrete-in-workflow`；targets `WF-R008, WF-R010`；excluded `WF-R002, WF-R015, WF-R024` | recipe owner 漂移跨 domain+application；不先收 recipe，`WF-R002` 无法真退出 | scope：`modules/workflow/domain/domain/recipes/**` 与 `application/usecases/recipes/**` concrete；non-scope：不动 process-core compat target、不动 apps recipe consumers | recipe canonical owner target 与 recipe public surface 已冻结 | actions：`move-concrete-out`, `remove-from-build`, `split-mixed-surface`；artifacts：recipe CMake、recipe headers、focused tests | accept：workflow 不再编译 recipe domain services 与 recipe CRUD/bundle usecases；validate：targeted build、recipe tests、include path check | rollback `medium`；blast `medium`；type `family-close`；next green `WF-B06`；next blocked `WF-B17` |
| `WF-B10` Dispensing Concrete Exit From Workflow | family `dispensing-concrete-in-workflow`；targets `WF-R006, WF-R011`；excluded `WF-R020, WF-R024, WF-R026` | dispensing family仍跨 domain payload 与 valve app concrete；继续留在 workflow 会长期污染 M0 | scope：`domain/domain/dispensing/**` 与 `application/usecases/dispensing/valve/**` concrete；non-scope：不动 deprecated dispense headers、不动 DXF adapter | canonical landing owner 已冻结于 `dispense-packaging` 或 runtime/device | actions：`move-concrete-out`, `remove-from-build`, `split-mixed-surface`；artifacts：dispensing CMake、valve headers/tests | accept：workflow 不再编译 purge/supply/valve coordination concrete 或 valve command/query usecases；validate：targeted build、valve/dispensing tests、include path check | rollback `medium`；blast `medium`；type `family-close`；next green `WF-B16`；next blocked `WF-B04` |
| `WF-B11` Safety Concrete Exit From Workflow Bridge Domain | family `safety-concrete-in-workflow`；targets `WF-R009`；excluded `WF-R003, WF-R021, WF-R025` | safety concrete 仍在 `domain/domain/safety/**`，bridge root 不能先 collapse | scope：workflow safety domain concrete；non-scope：不在本批中退出 motion-core compat target 或改 runtime-execution consumer | safety owner public surface 已冻结；`WF-B04`, `WF-B02` 路径已清 | actions：`move-concrete-out`, `remove-from-build`；artifacts：safety domain CMake、headers、runtime tests | accept：workflow bridge-domain 不再编译 `EmergencyStopService.cpp`、`InterlockPolicy.cpp`、`SafetyOutputGuard.cpp`、`SoftLimitValidator.cpp`；validate：targeted build、安全测试、include path check | rollback `large`；blast `large`；type `family-close`；next green `WF-B07`；next blocked `WF-B02` |
| `WF-B12` Motion Execution Concrete Exit From Workflow | family `motion-execution-in-workflow`；targets `WF-R013`；excluded `WF-R021, WF-R024, WF-R025, WF-R014` | motion residue blast 最大，必须独立成批；不能混入 naming 或 app consumer 改动 | scope：`application/usecases/motion/**` 与 `application/execution-supervision/runtime-consumer/**`；non-scope：不做 `siligen_control_application` 命名清理、不改外部 apps consumer | runtime-execution application public 与 motion-planning contracts 已冻结 | actions：`move-concrete-out`, `split-mixed-surface`, `remove-from-build`；artifacts：motion usecases、runtime assembly factory、CMake、focused tests | accept：workflow application 不再编译 motion execution/control concrete，也不再拥有 `MotionRuntimeAssemblyFactory`；validate：targeted build、motion tests、include path check | rollback `large`；blast `large`；type `family-close`；next green `WF-B22`；next blocked `WF-B17` |
| `WF-B13` System Concrete Exit From Workflow | family `system-concrete-in-workflow`；targets `WF-R012`；excluded `WF-R019, WF-R024, WF-R014` | system usecase 的 app-local vs module-owner 边界仍混；必须独立冻结 | scope：`application/usecases/system/**` concrete；non-scope：不做 naming/doc cleanup，不大改 runtime-service wiring | system landing 已冻结于 `apps/runtime-service` 或 `modules/runtime-execution` | actions：`move-concrete-out`, `remove-from-build`, `split-mixed-surface`；artifacts：system headers、CMake、app wiring tests | accept：workflow application 不再编译 system concrete usecases，workflow system 头不再导出 runtime/system contracts；validate：targeted build、system tests、include path check | rollback `medium`；blast `medium`；type `family-close`；next green `WF-B22`；next blocked `WF-B03` |
| `WF-B14` Machine/Calibration Concrete Split Out | family `machine-calibration-in-workflow`；targets `WF-R007`；excluded `WF-R005, WF-R014` | 单个 residue 内同时混了 `CalibrationProcess` 与 machine aggregate，必须先 split 责任 | scope：`domain/domain/machine/**`；non-scope：不 collapse `domain/domain` root、不改无关 machine consumers | split 轴已冻结于 `coordinate-alignment` 与非-workflow machine owner | actions：`split-mixed-surface`, `move-concrete-out`, `remove-from-build`；artifacts：machine domain CMake、aggregate headers、alignment tests | accept：workflow 不再同时承载 `CalibrationProcess` 与 machine aggregate concrete；validate：targeted build、machine/alignment tests | rollback `medium`；blast `medium`；type `family-close`；next green `WF-B16`；next blocked `-` |
| `WF-B15` DXF Parsing Adapter Exit From Workflow | family `dxf-adapter-in-workflow`；targets `WF-R026`；excluded `WF-R015, WF-R024, WF-R031` | adapter concrete 与 workflow orchestration 无 owner 关系，且验证路径独立，适合单独成批 | scope：`adapters/infrastructure/adapters/planning/dxf/**`；non-scope：不动 offline engineering Python tools、不改 apps consumer | landing owner 与 protobuf generation 边界已冻结 | actions：`move-concrete-out`, `remove-from-build`；artifacts：adapter CMake、protobuf wiring、DXF tests | accept：workflow adapters 不再编译 `PbPathSourceAdapter.cpp`、`AutoPathSourceAdapter.cpp`、`DXFAdapterFactory.cpp`；validate：targeted build、DXF/adapter tests | rollback `medium`；blast `medium`；type `family-close`；next green `WF-B21`；next blocked `-` |
| `WF-B16` Bridge-Domain Root Collapse | family `bridge-domain-live-payload-root`；targets `WF-R005`；excluded `WF-R006, WF-R007, WF-R008, WF-R009` | `domain/domain` 根目录只能在 child payload family 清空后退场 | scope：`modules/workflow/domain/domain/CMakeLists.txt` bridge root；non-scope：不在本批中移动 remaining concrete | `WF-B09`, `WF-B10`, `WF-B11`, `WF-B14` green | actions：`collapse-shell`, `convert-to-forward-only`, `delete-deprecated-surface`, `remove-from-build`；artifacts：`domain/domain/CMakeLists.txt`、bridge shell scaffolding | accept：`domain/domain` 不再定义 live foreign concrete target；剩余内容仅 forward-only/deprecated shell 或退出 build；validate：target graph check、targeted build | rollback `large`；blast `large`；type `family-close`；next green `WF-B08`；next blocked `WF-B09` |

## 4.3 P2 batches
| Batch | Family / Targets / Excluded | Why This Batch First | Scope / Non-scope | Preconditions | Actions / Artifacts | Acceptance / Validation | Risk / Type / Next |
|---|---|---|---|---|---|---|---|
| `WF-B17` External App Consumer Retarget Off Workflow Old Surfaces | family `external-consumer-old-workflow-includes`；targets `WF-R024`；excluded `WF-R015, WF-R016, WF-R017, WF-R025` | consumer retarget 是 producer surface cleanup 的后手，不是先手；且 `WF-R024` 目前过粗 | scope：`apps/planner-cli/**`、`apps/runtime-service/**`、`apps/runtime-gateway/transport-gateway/**` 中仍吃 workflow 旧 surface 的 include/link；non-scope：不改 workflow producer targets、不做 naming/doc cleanup | `WF-B03`, `WF-B04` 和相关 owner concrete batches green；`WF-R024` 已按 owner slice 拆分 | actions：`retarget`, `delete-deprecated-surface`；artifacts：app container/wiring、CLI handlers、gateway facade headers | accept：apps 不再通过 `application/usecases/*` 或 `workflow/application/usecases/recipes/*` 获取 live workflow/foreign owner 语义；validate：targeted app builds、include path check、smoke tests | rollback `large`；blast `large`；type `partial-family-close`；next green `WF-B18`；next blocked `WF-B03` |
| `WF-B18` Workflow Canonical Tests Boundary Cleanup | family `workflow-tests-foreign-deps`；targets `WF-R027`；excluded `WF-R024, WF-R029, WF-R032` | tests 只能在 producer/consumer 都收口后再跟进；提前做只会制造伪回归 | scope：`modules/workflow/tests/canonical/**`；non-scope：不编辑 owner-module 测试，不把 gate 修复混入测试批次 | `WF-B03~WF-B17` 相关 family 已绿 | actions：`retarget`, `remove-from-build`, `add-temporary-guardrail`；artifacts：tests CMake、test includes、test target links | accept：workflow canonical tests 不再依赖 process-core/motion-core include roots 或 foreign owner public/runtime concrete；validate：targeted test build、workflow unit/integration slice | rollback `medium`；blast `medium`；type `family-close`；next green `WF-B20`；next blocked `WF-B17` |
| `WF-B19` Legacy Planning/System Compat Consumer Closure | family `legacy-planning-system-consumers`；targets `WF-R030`；excluded `WF-R022, WF-R023, WF-R025` | 这是 repo 级 include graph trace 问题，不能在未完成 consumer trace 时直接执行 | scope：repo 内所有仍包含旧 `domain/system/*`、`domain/planning/*` 的 live consumers；non-scope：不改 canonical 目录命名，不动无关 headers | `WF-B04` green；include graph trace 完整 | actions：`retarget`, `delete-deprecated-surface`；artifacts：affected include sites、compat headers | accept：repo 内不再存在通过旧 `domain/system/*` / `domain/planning/*` 获取 live 语义的消费者；validate：repo-wide include path check、targeted builds | rollback `medium`；blast `medium`；type `family-close`；next green `WF-B20`；next blocked `WF-B04` |
| `WF-B20` Validation Tooling And Exit Gate Cleanup | family `false-clean-governance`；targets `WF-R029, WF-R032`；excluded `WF-R028, WF-R001, WF-R017` | gate/validator 必须反映清理后的真状态；不能提前改脚本去“配合”脏结构 | scope：`scripts/migration/validate_workspace_layout.py`、`legacy-exit-check.ps1`、`ci.ps1`、`scripts/migration/legacy_fact_catalog.json`；non-scope：不使用脚本变化去掩盖未关掉的 target/surface residue | `WF-B01`, `WF-B02`, `WF-B05`, `WF-B08` 与相关 family 已绿；缺失 trace 已补齐 | actions：`add-temporary-guardrail`, `retarget`, `delete-deprecated-surface`；artifacts：validation scripts、fact catalog、CI gate config | accept：validator 不再要求 workflow foreign wiring；exit gate 能机械拦截 live `siligen_domain` / `siligen_process_core` / `siligen_motion_core` / `siligen_workflow_*_public` residue；validate：legacy-exit、CI dry-run、script checks | rollback `medium`；blast `medium`；type `family-close`；next green `WF-B23`；next blocked `WF-B01` |
| `WF-B21` Offline Engineering Tools Owner Freeze | family `offline-engineering-tools-in-workflow`；targets `WF-R031`；excluded `WF-R026, WF-R024` | 这是 owner trace 问题，不是单纯路径问题；必须先冻结入口归属 | scope：`application/engineering_preview_tools/**`、`engineering_trajectory_tools/**`、`engineering_simulation_tools/**`；non-scope：不动在线 workflow orchestration，不把 DXF adapter 混入 | call-chain 与 canonical engineering owner 已冻结 | actions：`move-concrete-out`, `remove-from-build`, `retarget`；artifacts：Python entrypoints、tool packaging、相关 tests | accept：这些 offline engineering tools 不再常驻 `modules/workflow/application`，除非能证明其语义就是 M0 orchestration；validate：Python tests、entrypoint path check | rollback `medium`；blast `medium`；type `family-close`；next green `WF-B23`；next blocked `WF-B15` |

## 4.4 P3 batches
| Batch | Family / Targets / Excluded | Why This Batch First | Scope / Non-scope | Preconditions | Actions / Artifacts | Acceptance / Validation | Risk / Type / Next |
|---|---|---|---|---|---|---|---|
| `WF-B22` Workflow Application Naming Cleanup | family `application-mixed-naming`；targets `WF-R014`；excluded `WF-R010, WF-R011, WF-R012, WF-R013, WF-R028` | 命名只能在 concrete owner 收口后再处理；提前做只是“更好看的假完成” | scope：`modules/workflow/application/CMakeLists.txt` target/alias naming；non-scope：不移动 concrete，不写 docs | `WF-B12`, `WF-B13` 及其余 application owner batches green | actions：`rename-after-cleanup`, `split-mixed-surface`；artifacts：application CMake、exported target aliases、consumer link names | accept：不再存在单个 `siligen_control_application` / `siligen_application` 来掩盖多 owner concrete；validate：target/link check、targeted configure/build | rollback `medium`；blast `medium`；type `family-close`；next green `WF-B23`；next blocked `WF-B12` |
| `WF-B23` Workflow Narrative/Module Contract Correction | family `doc-contract-false-clean`；targets `WF-R028`；excluded `WF-R029, WF-R032, WF-R014` | 文档和 module narrative 只能在事实层都对齐后再写回 | scope：`README.md`、`module.yaml`、相关 CMake 注释；non-scope：不以文档改动代替代码/build/public surface cleanup | 所有相关 factual batches green；`WF-B20` green | actions：`retarget`, `rename-after-cleanup`；artifacts：README、module.yaml、CMake comments | accept：README/module.yaml/CMake comments 与 live target/include/link 事实一致，不再提前宣告 bridge-only closeout；validate：doc fact check vs target graph | rollback `small`；blast `small`；type `family-close`；next green `-`；next blocked `WF-B20` |

# 5. Deferred or blocked batches
- 因为 residue 仍是 `suspected`：
  - `WF-B19`：`WF-R030` 仍缺 repo 级 include graph 证据。
  - `WF-B20`：`WF-R029`、`WF-R032` 都是治理/gate suspected residue。
  - `WF-B21`：`WF-R031` 仍缺入口 owner 冻结。
- 因为缺少 target dependency trace：
  - `WF-B06`：`WF-R002` 的 recipe landing target 未冻结。
  - `WF-B07`：`WF-R003` 的 safety/runtime landing 与 downstream link 仍未冻结。
  - `WF-B09`：recipe family 的 canonical owner target 未冻结。
  - `WF-B10`：dispensing/valve landing owner 在 `dispense-packaging` 与 runtime/device 之间仍未冻结。
  - `WF-B13`：system usecase 应落 app-local 还是 `runtime-execution` 仍未冻结。
  - `WF-B15`：DXF adapter 的 landing owner 与 protobuf 边界未冻结。
- 因为缺少 include graph trace：
  - `WF-B03`：`WF-R015` 会触发 app consumer 收缩，但 `WF-R024` 还未按 owner slice 拆开。
  - `WF-B04`：`WF-R020/021/016` 会触发 runtime-execution 与潜在 repo consumers，trace 还不完整。
  - `WF-B17`：`WF-R024` 本身就是一个未拆开的粗粒度 consumer residue。
  - `WF-B19`：`WF-R030` 的全量消费者未知。
- 因为 blast radius 过大需要先拆批：
  - `WF-B12`：`WF-R013` 同时混了 motion usecases 与 runtime assembly；若 trace 进一步显示跨 owner 差异，应继续拆成 `runtime-assembly` 与 `motion-usecases` 两批。
  - `WF-B17`：`WF-R024` 至少要拆为 `motion/system/dispensing/recipe/DXF` 消费者切片后才适合执行。
- 因为会把 root cause 和 symptom 混在一起：
  - `WF-B14`：`WF-R007` 内同时混了 calibration owner 与 machine aggregate concrete，先 split 责任轴，后做迁出。
  - `WF-B16`：`WF-R005` 不能先做，否则只是 collapse symptom shell，保留 child root cause。
- 因为依赖前置批次尚未完成：
  - `WF-B02` 依赖 `WF-B04`
  - `WF-B05` 依赖 `WF-B03` + `WF-B04`
  - `WF-B08` 依赖 `WF-B01` + `WF-B02` + `WF-B06` + `WF-B07` + `WF-B16`
  - `WF-B11` 依赖 `WF-B04` + `WF-B02`
  - `WF-B18` 依赖 `WF-B03~WF-B17` 的事实收口
  - `WF-B22` 依赖 `WF-B12/13` 等 application concrete family 关闭
  - `WF-B23` 依赖 `WF-B20`
- 当前主计划中没有“验收条件不可机械判断”的 batch；所有 batch 的 acceptance 都能写成 target/include/link/test/gate 级别的机械判定。问题在于若干批次的前置 trace 仍未完成。

# 6. Recommended first execution batch
- 最适合作为第一批执行的是 `WF-B01`。
- 原因：
  - 它直接切 root build graph 对 workflow legacy provider 的假设，符合“先 build graph，后代码壳层”。
  - 它是明确 root cause，不是 symptom。
  - scope 最窄，集中在 root `CMakeLists.txt`，验收条件可机械判定。
  - 它不依赖未冻结的 landing owner，也不要求先补 include graph trace。
- 其它 batch 暂时不应先做：
  - `WF-B02~WF-B05` 仍依赖 public surface trace 或 consumer slice 拆分，直接做容易把 `WF-R024/025` 的粗粒度 residue 一起拖进来。
  - `WF-B06~WF-B16` 普遍依赖 canonical landing owner/target 冻结；先做会把“退出 workflow”偷换成“临时挪个壳”。
  - `WF-B17~WF-B21` 本质是 consumer/gate/trace 批，不应先于 producer/root build residue。
  - `WF-B22~WF-B23` 都是症状层，先做会制造假完成。
- `WF-B01` 若通过，链式最优顺序建议：
  - 先补足并细化 `WF-B04` 的 domain include trace，然后推进 `WF-B04 -> WF-B02 -> WF-B07 -> WF-B11 -> WF-B16 -> WF-B08`
  - 并行地补足 `WF-B03` 的 application consumer trace，再推进 `WF-B03 -> WF-B17 -> WF-B05`
  - 之后按 owner family 推进 `WF-B09 -> WF-B06`、`WF-B10`、`WF-B12`、`WF-B13`、`WF-B14`、`WF-B15`
  - 最后收 `WF-B18 -> WF-B20 -> WF-B22 -> WF-B23`

# 7. Anti-fake-completion rules for execution phase
- 未关闭该 batch 的 `target_residue_ids`，不得宣称该 batch 完成。
- 该 batch 的 `required_validation` 未全绿，不得宣称该 batch 完成。
- 不得通过新增 wrapper、alias、bridge、compat target、compat header，把 batch 内问题转嫁到别处后宣称完成。
- 不得只关闭 symptom 而保留 root cause；若 `root_or_symptom` 为 `mixed`，必须先证明 root 面已关。
- 若执行中发现新 residue，必须先登记到新 ledger/增补清单，判断是否属于当前 batch scope；未经登记不得顺手处理。
- 任何超出 `scope_statement` 的代码改动，必须在执行报告里单列为 out-of-scope changes；否则该批次视为边界失效。
- `family-close` 与 `module-clean-exit` 严禁混用；任一 batch 完成都不等价于 `modules/workflow` 已 clean-exit。
- `partial-family-close` 批次完成后，必须显式列出 family 内剩余 residue；不得把“family 已拆小”误报成“family 已关闭”。
- `prerequisite-batch` 完成只代表下游 batch 可进入执行前置条件，不代表业务 residue 已经关闭。

# 8. Completion verdict
`batch-plan-partial-with-gaps`
