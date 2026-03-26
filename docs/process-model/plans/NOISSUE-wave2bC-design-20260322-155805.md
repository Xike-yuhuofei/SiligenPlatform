# NOISSUE Wave 2B-C 设计：Residual Dependency Owner And Payoff

- 状态：Done
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-155805`，`branchSafe=feat-dispense-NOISSUE-e2e-flow-spec-alignment`

## 范围

1. 基于 `tools/migration/wave2b_residual_ledger.json` 和 `tools/migration/validate_wave2b_residuals.py`，识别当前 5 条 residual 的真实语义。
2. 基于 `packages/process-runtime-core`、`packages/device-adapters`、`packages/device-contracts` 及相关 `README` / `BOUNDARIES.md` / `DEPENDENCY_AUDIT.md`，给出每条 residual 的类型、owner 和 payoff 顺序。
3. 明确哪些 residual 是文档残留，哪些 residual 虽然当前只在文档命中，但其背后对应的仍是 live runtime/build dependency。
4. 给出 ledger 分阶段收紧方案和 gate 收紧方案。

## 非范围

1. 不修改 `tools/migration/wave2b_residual_ledger.json`、`tools/migration/validate_wave2b_residuals.py` 或任何代码。
2. 不直接删除 allowlist，也不直接给出“现在就删某条 ledger entry”的结论。
3. 不执行 `Wave 2B-A` root build/test/source-root cutover、`Wave 2B-B` runtime 默认资产路径切换或 `Wave 2B-D` gate 落地实现。
4. 不把 HMI 历史依赖审计条目误判成当前运行时依赖。

## 基线结论

1. `python tools/migration/validate_wave2b_residuals.py` 当前通过，说明 ledger 与现状一致；这只表示“防新增”有效，不表示 residual 已经完成清零。
2. 当前 5 条条目里，`process-runtime-core-infrastructure-runtime`、`hmi-multicard-driver-probe`、`hmi-vendor-dll-probe` 是文档残留。
3. `process-runtime-core-device-hal`、`device-adapters-third-party` 是“账本命中在文档、真实 blocker 在 runtime/build owner”的混合条目；不能把 README/AUDIT 中的叙述当成真正 debt owner。
4. `packages/device-contracts` 根据 `packages/device-contracts/README.md` 的边界说明，当前不应被分配 residual payoff 实施责任；它是已脱离 legacy 的边界见证者，不是剩余 debt owner。

## residual 分类表

| ledger id | pattern | 当前命中文件 | residual 类型 | 当前含义 | 切换前必须清零 |
|---|---|---|---|---|---|
| `process-runtime-core-infrastructure-runtime` | `control-core/src/infrastructure/runtime` | `packages/process-runtime-core/BOUNDARIES.md`、`packages/simulation-engine/BOUNDARIES.md` | 文档残留 / 禁止依赖边界说明 | 两个包都在文档里声明“禁止依赖 legacy runtime root”；未见当前 live code 命中证据 | 否 |
| `process-runtime-core-device-hal` | `control-core/modules/device-hal` | `packages/process-runtime-core/BOUNDARIES.md`、`packages/device-adapters/README.md`、`packages/traceability-observability/README.md` | 混合条目：文档残留 + 真实 owner debt 占位 | 文档命中本身不是 blocker；真正 blocker 是 diagnostics/logging 与 recipes persistence/serializer 仍以 legacy `device-hal` 语义挂账 | 是 |
| `device-adapters-third-party` | `control-core/third_party` | `packages/device-adapters/README.md`、`packages/shared-kernel/README.md`、`apps/hmi-app/DEPENDENCY_AUDIT.md` | 混合条目：文档残留 + 真实 third-party consumer debt 占位 | 文档命中本身不是 blocker；真正 blocker 是 `process-runtime-core` 等真实消费者仍需要把 third-party owner 从 legacy 语义切成 consumer-owned | 是 |
| `hmi-multicard-driver-probe` | `control-core/src/infrastructure/drivers/multicard` | `apps/hmi-app/DEPENDENCY_AUDIT.md` | HMI 历史依赖审计残留 | 仅用于记录“已移除的非法依赖”；不是当前运行时探测链路 | 否 |
| `hmi-vendor-dll-probe` | `control-core/third_party/vendor/MultiCardDemoVC/bin` | `apps/hmi-app/DEPENDENCY_AUDIT.md` | HMI 历史依赖审计残留 | 仅用于记录“已移除的非法依赖”；不是当前运行时 DLL 探测链路 | 否 |

## owner 表

| residual / payoff 单元 | 主 owner | 协作 owner | 依据 | 退出条件 |
|---|---|---|---|---|
| `process-runtime-core-infrastructure-runtime` 文档归并 | `packages/process-runtime-core` | `packages/simulation-engine` | 两处命中都在各自边界文档；这是边界表述归并，不是运行时代码清理 | 只保留单一 canonical 边界说明，或在最终 legacy 归零时移除该历史提法 |
| `process-runtime-core-device-hal` / diagnostics logging payoff | `packages/traceability-observability` | `packages/runtime-host` | `packages/traceability-observability/README.md` 已承认 logging 来源含 `device-hal diagnostics`；`docs/architecture/device-hal-cutover.md` 明确 logging 不应继续留在 device 层 | `SpdlogLoggingAdapter` 不再以 `modules/device-hal` 路径存在；consumer 改指向 canonical logging owner |
| `process-runtime-core-device-hal` / recipes persistence payoff | `packages/runtime-host` | `packages/transport-gateway` | `docs/architecture/device-hal-cutover.md` 明确 recipes persistence/serializer 不应继续留在 device 层，且 `TcpCommandDispatcher` 仍吃 `RecipeJsonSerializer` | `Recipe*` / `Template*` / `Audit*` / `ParameterSchema*` / `RecipeBundleSerializer*` 不再由 legacy `device-hal` 提供 |
| `process-runtime-core-device-hal` / device boundary 守门 | `packages/device-adapters` | `packages/process-runtime-core` | `packages/device-adapters/README.md` 已明确“残留不回灌本包”；该包的责任是继续守边界，不是接收 recipes/logging | README 和边界门禁不再需要保留 `device-hal` 残留说明 |
| `device-adapters-third-party` / Boost、Ruckig、Protobuf consumer payoff | `packages/process-runtime-core` | `packages/shared-kernel` | `docs/architecture/third-party-ownership-cutover.md` 明确 live consumer 主要在 `process-runtime-core`；`shared-kernel` 已是已脱离见证者，不应重新背锅 | `process-runtime-core` 不再把 `control-core/third_party` 当临时 owner；依赖改为显式 consumer-owned |
| `device-adapters-third-party` / JSON + recipe serializer payoff | `packages/runtime-host` | `packages/transport-gateway` | `third-party-ownership-cutover.md` 明确 `json` 与 recipe serializer owner 切换耦合，`TcpCommandDispatcher` 是直接协作方 | recipe serializer owner 固化后，`transport-gateway` 不再借 `control-core` third-party/include root 暴露 json |
| `device-adapters-third-party` / vendor 代码 owner 见证 | `packages/device-adapters` | `apps/hmi-app` | `packages/device-adapters/vendor/multicard/README.md` 已说明 vendor 资产 canonical 位置；HMI audit 仅保留已移除事实 | README/AUDIT 不再需要通过 `control-core/third_party` 叙述 provenance |
| `hmi-multicard-driver-probe` | `apps/hmi-app` | `packages/device-adapters` | 命中只在 HMI 审计文档；MultiCard 代码 owner 已在 `packages/device-adapters` | HMI audit 迁成“当前 canonical 依赖”表述后删除历史 probe 叙述 |
| `hmi-vendor-dll-probe` | `apps/hmi-app` | `packages/device-adapters` | 命中只在 HMI 审计文档；DLL canonical owner 已在 `packages/device-adapters/vendor/multicard` | HMI launcher / deploy 文档只保留 canonical vendor 来源，不再记录 legacy DLL probe |

## 清账优先级

| 优先级 | 事项 | 为什么 | 是否切换前必须完成 |
|---|---|---|---|
| `P0` | 冻结 recipes persistence/serializer canonical owner | 这是 `process-runtime-core-device-hal` 的核心 blocker；不先定 owner，就无法切 include/link，也无法拆 `json` debt | 是 |
| `P0` | 冻结 diagnostics logging canonical owner | `device-hal` 剩余 debt 之一；若继续模糊挂在 device 层，会导致 owner 错位 | 是 |
| `P0` | 冻结 `process-runtime-core` 的 third-party consumer-owned 方案 | `Boost` / `Ruckig` / `Protobuf` 仍是实际 build graph blocker；不清零就不能把 `third_party` 从 legacy owner 语义上摘掉 | 是 |
| `P1` | 把 `process-runtime-core-device-hal` 拆成 logging / recipes / doc-guard 三个 payoff 单元 | 当前 ledger 条目过粗，无法直接派工，也会误伤 `device-adapters` | 是 |
| `P1` | 把 `device-adapters-third-party` 拆成 runtime/build consumer debt 与文档见证 debt | 当前条目把 `process-runtime-core`、HMI audit、shared-kernel witness 混在一起 | 是 |
| `P2` | 归并 `process-runtime-core-infrastructure-runtime` 的边界文档表述 | 这是文档 debt，不阻塞 runtime cutover，但会阻塞最终 ledger 归零 | 否 |
| `P2` | 清理 HMI audit 中两条历史 probe 叙述 | 不影响切换，但最终账本不应永久保留历史探测说明 | 否 |

## gate 收紧计划

### 阶段 0：当前基线

1. 保持现有 validator 语义不变：目标是阻止新增 live 引用，不强迫历史文档立即归零。
2. 维持当前结论：`entry_count=5`，`present_entry_count=5`，`issue_count=0`。

### 阶段 1：先按语义拆账，不直接删账

1. 把 `process-runtime-core-device-hal` 拆成：
   - `device-hal-doc-boundary`
   - `device-hal-logging-owner`
   - `device-hal-recipes-owner`
2. 把 `device-adapters-third-party` 拆成：
   - `third-party-doc-boundary`
   - `process-runtime-core-third-party-consumer`
   - `recipe-json-third-party-consumer`
   - `hmi-vendor-provenance-doc`
3. 这一阶段只做“条目收窄”和“owner 显式化”，不做 allowlist 直接删除。

### 阶段 2：先把 live code/build residual 收到 0

1. 对 `packages/process-runtime-core`、`packages/runtime-host`、`packages/transport-gateway`、`packages/device-adapters`、`packages/device-contracts` 做 code/CMake/script 范围零命中门禁。
2. 允许历史事实继续留在 README / `BOUNDARIES.md` / `DEPENDENCY_AUDIT.md`，但不允许新增到 `.cpp`、`.h`、`.hpp`、`.py`、`.ps1`、`.cmake`、`CMakeLists.txt`。
3. 阶段 2 通过标准：runtime/build residual 归零，文档 residual 可暂留。

### 阶段 3：把文档 residual 收敛到单一证据点

1. 每个 residual family 只允许一个 canonical 说明位置。
2. README 里只保留当前边界结论，不再重复记录 legacy provenance 细节。
3. HMI audit 只保留当前合法依赖和替代入口，不再保留历史 probe 名称。

### 阶段 4：最后再移除 ledger 条目

1. 只有同时满足“runtime/build 归零”和“文档已归并或改写”为当前事实，才允许移除对应 allowlist entry。
2. 允许删除顺序按 family 分批推进，不要求 5 条一次性同时删除。

## 验证命令与命中预期

### 当前基线命令

1. `python tools/migration/validate_wave2b_residuals.py`
   - 预期：`entry_count=5`，`present_entry_count=5`，`issue_count=0`
2. `rg -n "control-core/src/infrastructure/runtime" packages/process-runtime-core/BOUNDARIES.md packages/simulation-engine/BOUNDARIES.md`
   - 预期：2 处命中，且都在边界文档
3. `rg -n "control-core/modules/device-hal" packages/process-runtime-core/BOUNDARIES.md packages/device-adapters/README.md packages/traceability-observability/README.md`
   - 预期：3 处命中，且都在文档
4. `rg -n "control-core/third_party" packages/device-adapters/README.md packages/shared-kernel/README.md apps/hmi-app/DEPENDENCY_AUDIT.md`
   - 预期：3 处命中，且都在文档
5. `rg -n "control-core/src/infrastructure/drivers/multicard|control-core/third_party/vendor/MultiCardDemoVC/bin" apps/hmi-app/DEPENDENCY_AUDIT.md`
   - 预期：各 1 处命中，且都在“已移除的非法依赖”区段

### 阶段 2 应新增的零命中验证

1. `rg -n "control-core/modules/device-hal|control-core/third_party" packages/process-runtime-core packages/runtime-host packages/transport-gateway packages/device-adapters packages/device-contracts -g "*.cpp" -g "*.cc" -g "*.h" -g "*.hpp" -g "*.py" -g "*.ps1" -g "*.cmake" -g "CMakeLists.txt"`
   - 预期：0 命中
2. `rg -n "control-core/src/infrastructure/runtime" packages/process-runtime-core packages/simulation-engine -g "*.cpp" -g "*.cc" -g "*.h" -g "*.hpp" -g "*.py" -g "*.ps1" -g "*.cmake" -g "CMakeLists.txt"`
   - 预期：0 命中
3. `rg -n "control-core/src/infrastructure/drivers/multicard|control-core/third_party/vendor/MultiCardDemoVC/bin" apps/hmi-app -g "*.py" -g "*.ps1" -g "*.json" -g "*.md"`
   - 预期：阶段 2 允许只剩 `DEPENDENCY_AUDIT.md`；进入阶段 3 后归零

## 风险

1. 继续用当前粗粒度 ledger 直接派工，会把 `device-hal` debt 错派给 `device-adapters`，把 `third_party` debt 错派给 HMI 或 shared-kernel。
2. 如果先删 README/AUDIT 中的 residual 提法，而 runtime/build owner 还没切走，会出现“文档干净但真实 debt 还在”的假清零。
3. 如果把 HMI 两条历史 probe 当成运行时 blocker，会无谓阻塞 Wave 2B 切换节奏。
4. 如果不先冻结 recipes/logging 的 canonical owner，`device-hal` residual 无法收口，`json` 与 `third_party` debt 也会继续串联。
5. 如果 `process-runtime-core` 的 third-party 仍保持 legacy owner 语义，即使 `device-adapters` 和 `device-contracts` 已经脱离，也无法完成最终 `control-core` provenance 收口。

## 阻断项

1. `recipes persistence/serializer` 的 canonical owner 还没有冻结成单一结论；现有资料只指向“`packages/runtime-host` 或独立 package”，这会阻断 `device-hal` payoff 派工。
2. `diagnostics logging` 的 canonical owner 当前只有 `packages/traceability-observability` 占位描述，尚缺正式落点冻结。
3. `third-party` 的 consumer-owned 注册方式还没有冻结成单一机制；`Boost`、`Ruckig`、`Protobuf`、`json` 不能在没有统一注入方案时分别各自漂移。
4. 当前 ledger schema 只有 `id`、`pattern`、`allowed_files`，没有 `type`、`owner`、`must_zero_before_cutover` 字段；如果不先补充设计约束，后续 gate 只能做字符串命中，无法表达切换顺序。

## 最终回答

1. 每条 residual 的 owner 已在上表按“账本条目 / payoff 单元”拆清；其中 `device-contracts` 当前不是 residual payoff owner。
2. 每条 residual 的类型已分成三类：文档残留、HMI 历史审计残留、文档命中但背后对应真实 runtime/build debt 的混合条目。
3. 切换前必须清零的是两类真实 debt：`process-runtime-core-device-hal` 背后的 logging/recipes owner debt，以及 `device-adapters-third-party` 背后的 third-party consumer debt；纯文档 residual 不要求在切换前先清零。
4. ledger 收紧顺序应为：先拆账显式化 owner，再把 runtime/build 命中收敛到 0，再归并文档命中，最后才允许删除对应 allowlist 条目。
