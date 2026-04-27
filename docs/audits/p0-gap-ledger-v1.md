# P0 Gap Ledger v1

## 1. 总体结论

本轮基于仓库现状做只读审查，不改代码。

结论先行：当前上位机在本轮限定的 4 项里，并不是“四项都缺”。`DXF 输入治理`、`预览执行一致性`、`设备执行安全` 都已经有明确 owner 和多处 fail-closed 门禁；真正构成“产品级可靠 + 算法领先基础版”最小 P0 差距的，集中在 2 个地方：

1. 缺少 runtime-owned 的路径质量 authority/gate。现有 `dxf.plan.prepare` 主要输出导入诊断、spacing 诊断、`formal_compare_gate` 和预览元数据，但没有一个正式的“路径质量 verdict”来判定 fragmented / discontinuity / micro-segment burst 这类低质量路径是否允许生产。
2. `soft limit` 安全线没有接进正式 DXF 执行主链。`SoftLimitValidator` 和 `SoftLimitMonitorService` 已存在，也各自有单测，但当前代码证据只看到定义、构建和单测，没有看到它们接入 `dxf.job.start` 前置校验或 runtime-service 容器启动链。相对地，`HardLimitMonitorService` 已经完成了容器接线，并被 `InitializeSystemUseCase` 显式启动。

因此，当前最小 P0 差距不在 UI 美化、MES、云平台、多机管理，而在：

- 让“低质量但仍可被 current pipeline 规划出来的路径”在 owner contract 层被正式定义成不可生产。
- 让 `soft limit` 从“仓库里有类、脚本里有 preflight”升级成“产品执行链自身的正式安全门禁”。

## 2. P0 风险总表

| 编号 | 风险 | 所属链路 | 当前证据 | 缺失能力 | 风险等级 | 最小补齐动作 |
|---|---|---|---|---|---|---|
| P0-01 | 缺少 runtime-owned 路径质量 gate | `dxf.plan.prepare -> dxf.preview.snapshot -> dxf.job.start` | `PlanningUseCase.cpp` 已产出 `preview_validation_classification`、`preview_diagnostic_code`、`formal_compare_gate`；`DispensingWorkflowUseCase.cpp` 仅对 spacing / authority / binding / fingerprint fail-closed；`QualityMetrics.h` 存在高级指标字段但未被计划阶段消费 | 没有统一 `path_quality` authority surface，也没有把 `process_path_fragmentation` 等拓扑风险提升为正式 blocking gate | `P0` | 在 `prepare/start` contract 中增加单一 canonical `path_quality` verdict，并让 `dxf.job.start` 基于该 verdict fail-closed |
| P0-02 | `soft limit` 未接入 canonical execution safety | `dxf.job.start -> runtime-service motion host -> in-run stop` | `PlanningUseCase.cpp` 只做 plan-time bounds；`DispensingExecutionUseCase.Setup.cpp` 检查连接/回零/互锁但不检查 soft limit；`SoftLimitValidator.cpp`、`SoftLimitMonitorService.cpp` 仅见定义和单测；`ApplicationContainer.Motion.cpp` 只接了 hard-limit；`InitializeSystemUseCase.cpp` 也只显式启动 hard-limit monitor | 缺少 start 前 soft-limit 轨迹校验，也缺少 runtime host 的 soft-limit 监控启动 | `P0` | 在启动前用 `SoftLimitValidator` 校验 execution trajectory；在 runtime-service 容器中注册并启动 `SoftLimitMonitorService` |

## 3. DXF 输入治理审查

### 3.1 当前是否已有能力

有，且主链 owner 已明确下沉到 `job-ingest -> dxf-geometry -> runtime-execution`，不是 UI 侧拼装能力。

### 3.2 证据：文件路径、类、函数、测试、文档

- 代码证据
  - `modules/dxf-geometry/application/engineering_data/processing/dxf_to_pb.py`
    - `READABLE_DXF_VERSIONS` 明确版本白名单。
    - `STANDARD_CORE_ENTITY_TYPES`、`STANDARD_HIGH_ORDER_ENTITY_TYPES`、`IGNORED_ENTITY_TYPES`、`REJECTED_ENTITY_TYPES` 明确实体治理边界。
    - `resolve_unit_scale()` 在 `$INSUNITS=0` 时同时写入 `DXF_W_UNIT_ASSUMED_MM` 和 `DXF_E_UNIT_REQUIRED_FOR_PRODUCTION`。
    - `finalize_import_diagnostics()` 统一生成 `classification / preview_ready / production_ready / primary_code / warning_codes / error_codes`。
    - `main()` 对不支持版本写 `DXF_E_UNSUPPORTED_VERSION`，对高版本写 `DXF_W_HIGH_VERSION_DOWNGRADED`。
  - `modules/dxf-geometry/application/services/dxf/DxfPbPreparationService.cpp`
    - `ToImportDiagnosticsSummary()` 把 `.pb` 中的 `ImportDiagnostics` 还原为上层导入诊断 contract。
  - `modules/job-ingest/application/usecases/dispensing/UploadFileUseCase.cpp`
    - `UploadFileUseCase::Execute()` 只允许 `.dxf` 扩展名。
    - 真正 payload 解析和 prepared artifact 生成委托给 `preparation_port_->EnsurePreparedInput()`，没有在 upload 层做静默兼容。
  - `modules/runtime-execution/application/usecases/dispensing/DispensingWorkflowUseCase.cpp`
    - `DispensingWorkflowUseCase::StartJob()` 在启动前调用 `RefreshPlanImportDiagnostics()`。
    - 若 `response.import_diagnostics.production_ready == false`，直接返回 `INVALID_STATE`，错误文本优先对齐 `import_summary`。
- 测试证据
  - `tests/contracts/test_engineering_data_compatibility.py`
    - `test_dxf_to_pb_matches_fixture_semantics`
    - `test_dxf_to_pb_expands_insert_and_ignores_text_without_affecting_supported_entities`
    - `test_full_chain_canonical_cases_round_trip_to_their_fixtures`
  - `modules/job-ingest/tests/unit/dispensing/UploadFileUseCaseTest.cpp`
    - 覆盖 `preview_only + DXF_E_UNIT_REQUIRED_FOR_PRODUCTION` 诊断回传。
- 文档证据
  - `docs/architecture/dxf-input-contract-v1.md`
    - 冻结实体矩阵、错误码、块展开和单位治理。
  - `shared/contracts/application/commands/dxf.command-set.json`
    - `dxf.artifact.create`、`dxf.plan.prepare`、`dxf.job.start` 都要求返回 `import_result_classification / import_preview_ready / import_production_ready / import_* codes`。

### 3.3 缺失能力

- 未找到自动化负向回归，去冻结以下实现语义：
  - unsupported DXF version
  - unsupported unit code
  - rejected 3D entity
  - no valid entity
  - high-version downgrade warning
- 上传层当前只做扩展名校验，未找到独立文件头/内容签名预检的代码证据。当前依赖 `dxf_to_pb.py` 解析失败来 fail-closed。
- 未找到基于真实 import diagnostics 的 workflow/integration test，直接证明 `preview_only` 导入在运行时一定阻断 `dxf.job.start`。

### 3.4 是否属于 P0 风险

否。

原因不是“没有问题”，而是当前 owner 语义已经存在，主要缺口在 regression freeze，而不是生产门禁本身缺失。

### 3.5 是否会导致“不该执行却执行”

按当前代码证据，主执行链不会把 `import_production_ready=false` 的输入放行。

- 直接证据：`DispensingWorkflowUseCase::StartJob()` 在启动前检查 `response.import_diagnostics.production_ready`。
- 推断：只要 `dxf_to_pb.py` 产出 `preview_only/failed` 诊断，runtime 不会把它提升成 production-ready。

### 3.6 最小补齐路径

- 不先改 owner 语义，先把负向输入治理做成 contract regression。
- 把以下 5 类输入做成 canonical fixtures，并冻结到 `dxf_to_pb.py -> UploadFileUseCase -> dxf.plan.prepare/dxf.job.start` 的连续链路：
  - unsupported version
  - missing units
  - unsupported unit code
  - rejected 3D entity
  - no valid entity
- 如果后续要加 upload 前置校验，也应作为额外 fail-closed 检查存在，不能替代 `dxf_to_pb.py` 的正式诊断语义。

### 3.7 需要新增的测试

| 测试ID | 测什么 | 输入 | 期望输出 |
|---|---|---|---|
| DXF-GOV-01 | unsupported version 必须失败 | 一个 `dxfversion` 不在 `READABLE_DXF_VERSIONS` 的最小 DXF | `classification=failed`，`primary_code=DXF_E_UNSUPPORTED_VERSION`，`preview_ready=false`，`production_ready=false` |
| DXF-GOV-02 | missing units 只能 preview，不可 production | 一个只含 `LINE` 的最小 DXF，`$INSUNITS=0` | `classification=preview_only`，`primary_code=DXF_E_UNIT_REQUIRED_FOR_PRODUCTION`，`preview_ready=true`，`production_ready=false` |
| DXF-GOV-03 | rejected 3D entity 必须失败 | 一个含 `3DFACE` 的 DXF | `error_codes` 含 `DXF_E_3D_ENTITY_FOUND`，`classification=failed`，`production_ready=false` |
| DXF-GOV-04 | 无有效实体必须失败 | 一个只含 `TEXT/MTEXT` 的 DXF | `primary_code=DXF_E_NO_VALID_ENTITY`，`preview_ready=false`，`production_ready=false` |
| DXF-GOV-05 | 高版本降级必须留痕 | 一个 `AC1032/R2018` 的简单 2D DXF | `classification=success_with_warnings`，`warning_codes` 含 `DXF_W_HIGH_VERSION_DOWNGRADED`，`production_ready=true` |
| DXF-GOV-06 | `preview_only` 导入不得启动生产 | 先构造 `import_diagnostics.production_ready=false` 的 artifact，再走 `dxf.job.start` | `dxf.job.start` 返回错误，错误消息等于 `import_summary`，runtime start port 不被调用 |

### 3.8 后续可交给 Codex 的具体任务

- `test(dxf): freeze unsupported-version and unit-governance fixtures`
- `test(job-ingest): add rejected-3d and no-valid-entity import contract cases`
- `test(runtime): prove preview_only import blocks dxf.job.start end-to-end`
- `docs(dxf): sync dxf-input-contract-v1 with frozen negative fixture matrix`

## 4. 路径质量评价审查

### 4.1 当前是否已有能力

部分已有，但还不是“产品级路径质量 authority”。

当前更准确的状态是：仓库已有 path optimization、spacing 诊断、formal compare 诊断、fragmentation 元数据，但没有一个 runtime-owned 的最终路径质量 verdict 来决定“这条路径是否允许生产”。

### 4.2 证据：文件路径、类、函数、测试、文档

- 代码证据
  - `shared/contracts/application/commands/dxf.command-set.json`
    - `dxf.plan.prepare` 参数里有 `optimize_path`、`two_opt_iterations`。
    - 结果里没有 `path_quality_verdict`、`path_quality_reason_codes`、`path_quality_score` 一类正式 authority 字段。
    - shared contract 里也没有 `preview_spacing_valid` 或 `spacing_validation_groups` 的 wire surface。
  - `modules/dispense-packaging/application/usecases/dispensing/PlanningUseCase.cpp`
    - `PlanningUseCase::Execute()` 会产出 `preview_spacing_valid`、`preview_validation_classification`、`spacing_validation_groups`、`formal_compare_gate`。
    - `PlanningUseCase::PrepareAuthorityPreview()` 会记录 `discontinuity_count`，并在 `fragmentation_suspected` 时写 `preview_diagnostic_code = "process_path_fragmentation"`。
  - `modules/runtime-execution/application/usecases/dispensing/DispensingWorkflowUseCase.cpp`
    - `BuildPreviewGateDiagnostic()` 只对 `authority_ready`、`binding_ready`、`spacing_valid`、`validation_classification == "fail"`、`glue_point_count==0` 等条件 fail-closed。
    - 该函数不读取 `preview_diagnostic_code`，也不读取 `discontinuity_count`。
  - `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/dispensing/QualityMetrics.h`
    - 已定义 `line_width_cv`、`corner_deviation_pct`、`glue_pile_reduction_pct`、`trigger_interval_error_pct`。
  - `modules/runtime-execution/application/usecases/dispensing/DispensingExecutionUseCase.cpp`
    - 当前只填了 `quality_metrics.run_count` 和 `quality_metrics.interruption_count`，未见上述高级指标的真实计算或准入消费。
- 测试证据
  - `modules/dispense-packaging/tests/contract/application/services/dispensing/WorkflowPlanningAssemblyOperationsContractTest.cpp`
    - 覆盖 `preview_spacing_valid`
    - 覆盖 `preview_has_short_segment_exceptions`
    - 覆盖 `preview_validation_classification == "pass_with_exception"` / `"fail"`
  - `modules/runtime-execution/tests/unit/dispensing/DispensingWorkflowUseCaseTest.cpp`
    - `ConfirmPreviewAllowsPassWithException`
    - `GetPreviewSnapshotCarriesPreviewDiagnosticCode`
    - `StartJobRejectsFailPreviewUsingFailureReason`
    - 这些测试说明 `pass_with_exception` 可以进入 confirm，`process_path_fragmentation` 当前只是被带出，不是 start gate。
- 文档证据
  - `shared/contracts/application/mappings/protocol-mapping.md`
    - 明确 `preview_diagnostic_code` 属于“预览诊断元数据”，不能替代 authority 判定。

### 4.3 缺失能力

- 缺少一个 single-source-of-truth 的 `path_quality` authority surface。
- 缺少把现有拓扑诊断正式提升为 production gate 的规则：
  - `fragmentation_suspected`
  - `discontinuity_count`
  - 过密短段
  - 极端转角/急转
  - 其他会导致工艺不稳定但未被 spacing/fc gate 覆盖的路径问题
- 已存在的 `QualityMetrics` 没有在 plan-time 或 start-time 被真实计算和消费。

### 4.4 是否属于 P0 风险

是。

### 4.5 是否会导致“不该执行却执行”

会。

- 直接证据
  - `BuildPreviewGateDiagnostic()` 不读取 `preview_diagnostic_code`。
  - `protocol-mapping.md` 明确 `preview_diagnostic_code` 只是元数据，不是 authority。
  - `DispensingWorkflowUseCaseTest` 显示 `pass_with_exception` 可以被 `ConfirmPreview()` 接受。
- 推断
  - 只要路径没有触发 `spacing_valid=false` 或 `formal_compare_gate=production_blocked`，即使它已经被标成 `process_path_fragmentation`，当前主链仍可能继续到 `dxf.job.start`。

### 4.6 最小补齐路径

- 不做大重构，直接把现有诊断提升成一个单一 canonical contract，例如 `path_quality`：
  - `verdict`
  - `reason_codes`
  - `summary`
  - `blocking`
- 初版只复用现有 owner 已有信息，不引入第二套算法：
  - `preview_validation_classification`
  - `spacing_validation_groups`
  - `preview_diagnostic_code`
  - `discontinuity_count`
  - `formal_compare_gate`
- `dxf.job.start` 只认这一份 canonical `path_quality.blocking`，一旦 `true` 就 fail-closed。
- `preview_diagnostic_code` 不再只是“可显示元数据”，而是正式进入 `reason_codes`。

### 4.7 需要新增的测试

| 测试ID | 测什么 | 输入 | 期望输出 |
|---|---|---|---|
| PATH-Q-01 | fragmentation 不能只做元数据 | 一个能触发 `process_path_fragmentation`、但当前 spacing 仍可通过的 synthetic path | `dxf.plan.prepare.result.path_quality.blocking=true`，`reason_codes` 含 `process_path_fragmentation`，`dxf.job.start` 被拒绝 |
| PATH-Q-02 | clean baseline 路径必须通过 | `samples/dxf/rect_diag.dxf` | `path_quality.verdict=pass`，`reason_codes=[]`，`dxf.job.start` 允许启动 |
| PATH-Q-03 | 微短段爆发必须阻断生产 | 一个包含大量 `<阈值` 微短段、反复折返的 polyline DXF | `path_quality.blocking=true`，错误码稳定，`dxf.job.start` 不调用 runtime start |
| PATH-Q-04 | 显式治理的例外必须是显式、不可静默 | 一个现有 `short_segment_exception` 样例 | `path_quality.verdict=pass_with_exception`，`reason_codes` 明确包含例外原因；若例外不在 allowlist，则应改为 blocking |

### 4.8 后续可交给 Codex 的具体任务

- `fix(runtime): introduce canonical path_quality contract on dxf.plan.prepare and dxf.job.start`
- `fix(runtime): make preview_diagnostic_code/discontinuity_count participate in blocking verdict`
- `test(runtime): add fragmented-path and micro-segment negative start gates`
- `docs(contract): freeze path_quality semantics in shared protocol mapping`

## 5. 预览执行一致性审查

### 5.1 当前是否已有能力

有，而且相对成熟。

当前仓库已经把 `prepare -> snapshot -> confirm -> start` 做成了明确的 owner consistency gate，不是“先预览一份、启动再跑另一份”的松散链路。

### 5.2 证据：文件路径、类、函数、测试、文档

- 代码证据
  - `shared/contracts/application/mappings/protocol-mapping.md`
    - 明确 canonical 链是 `dxf.artifact.create -> dxf.plan.prepare -> dxf.preview.snapshot -> dxf.preview.confirm -> dxf.job.start -> dxf.job.status`。
    - 明确 `preview_source=planned_glue_snapshot`、`preview_kind=glue_points`、`motion_preview.source=execution_trajectory_snapshot`、`motion_preview.kind=polyline`。
  - `modules/runtime-execution/application/usecases/dispensing/DispensingWorkflowUseCase.cpp`
    - `ResolveStartJobExecutionLaunch()` 要求：
      - plan 存在且是 latest
      - preview state 已 confirmed
      - `preview_snapshot_hash == response.plan_fingerprint`
      - `request.plan_fingerprint == response.plan_fingerprint`
    - `BuildPreviewGateDiagnostic()` 会卡住 authority/binding/spacing/execution-binding 不一致。
  - `apps/runtime-gateway/transport-gateway/src/tcp/TcpCommandDispatcher.cpp`
    - `HandleDxfPreviewSnapshot()` 强校验：
      - `preview_source == "planned_glue_snapshot"`
      - `preview_kind == "glue_points"`
      - `preview_binding.source == "runtime_authority_preview_binding"`
      - `preview_binding` 与 `glue_points` 数量一致
      - `motion_preview_source == "execution_trajectory_snapshot"`
      - `motion_preview_kind == "polyline"`
  - `modules/runtime-execution/application/include/runtime_execution/application/usecases/dispensing/WorkflowExecutionPort.h`
    - 对 `PROFILE_COMPARE` 执行显式要求 `profile_compare_schedule + expected_trace`。
  - `modules/runtime-execution/application/usecases/dispensing/DispensingExecutionUseCase.Async.cpp`
    - `GetJobTraceability()` 仅允许 terminal state 读取。
    - 若 job 不携带 `expected_trace`，直接 fail-closed。
- 测试证据
  - `modules/runtime-execution/tests/unit/dispensing/DispensingWorkflowUseCaseTest.cpp`
    - `StartJobPreservesAuthorityFingerprintAndGluePoints`
    - `PreparePlanParameterChangeInvalidatesPreviouslyConfirmedPreview`
    - `StartJobRejectsPreviewBindingUnavailableBeforeRuntimeLaunch`
    - `StartJobRejectsUnhomedAxis`
  - `tests/integration/scenarios/first-layer/run_tcp_precondition_matrix.py`
    - `S2-dxf-job-start-without-preview-confirm` 明确把“未 confirm 就 start”当成失败场景。
  - `tests/integration/scenarios/first-layer/test_dxf_production_baseline_roundtrip.py`
    - 覆盖 `prepare -> snapshot -> confirm -> start` 的 baseline/fingerprint 连续性。
  - `tests/e2e/hardware-in-loop/run_real_dxf_preview_snapshot.py`
    - 在 HIL 下强校验 `preview_source`、`preview_kind`、`snapshot_hash`、`preview.confirm`。
- 文档证据
  - `shared/contracts/application/commands/dxf.command-set.json`
  - `shared/contracts/application/queries/dxf.query-set.json`
    - 已有 `dxf.job.traceability` 查询面。

### 5.3 缺失能力

- 对 `PROFILE_COMPARE` 之外的执行策略，未看到同等强度的 terminal strict proof。
- `dxf.job.traceability` 当前只在 job terminal 且携带 `expected_trace` 时成立；对 non-`PROFILE_COMPARE` 路径，严格逐点证明证据不足。
- 没有看到一个统一 release lane，把 `prepare -> snapshot -> confirm -> start -> traceability` 串成单一正式回归面，当前是多条测试分散覆盖。

### 5.4 是否属于 P0 风险

否。

### 5.5 是否会导致“不该执行却执行”

当前代码证据下，不会把它归类为主 P0 风险。

原因是当前缺口主要在“执行后证明深度”，不是“执行前授权边界”。当前 start gate 已对 fingerprint、preview confirm、authority/binding 一致性做 fail-closed。

### 5.6 最小补齐路径

- 保持现有 start gate 不变，不做回退。
- 先补一个统一 regression lane：
  - `PROFILE_COMPARE` 必须验证 `dxf.job.traceability.verdict=passed` 且 `strict_one_to_one_proven=true`
  - 非 `PROFILE_COMPARE` 至少验证 `plan_fingerprint`、`production_baseline`、`snapshot_hash` 在生命周期里不漂移
- 后续如需更强一致性证明，应按策略逐一扩展 traceability surface，不要引入 fallback 双轨。

### 5.7 需要新增的测试

| 测试ID | 测什么 | 输入 | 期望输出 |
|---|---|---|---|
| PREVIEW-CONS-01 | 参数变更后旧预览必须失效 | 同一 artifact 先 prepare/confirm，再用不同 spacing 参数重新 prepare | 旧 `plan_id` 的 `dxf.job.start` 返回 `plan is stale` |
| PREVIEW-CONS-02 | preview binding 破坏必须阻断 | 一个人为注入 `preview_binding.source != runtime_authority_preview_binding` 或计数不一致的 plan record | `dxf.preview.snapshot` 返回错误，`dxf.job.start` 也被拒绝 |
| PREVIEW-CONS-03 | PROFILE_COMPARE terminal traceability 必须闭环 | 一个 canonical `PROFILE_COMPARE` 计划在 mock/runtime 下完整执行到 terminal | `dxf.job.traceability.verdict=passed`，`strict_one_to_one_proven=true`，`expected_trace` 与 `actual_trace` 条目数一致 |
| PREVIEW-CONS-04 | non-PROFILE_COMPARE 也必须保持 lifecycle fingerprint 连续 | 一个 canonical flying-shot plan | `prepare.plan_fingerprint == snapshot.snapshot_hash == start.plan_fingerprint`，`production_baseline` 前后一致 |

### 5.8 后续可交给 Codex 的具体任务

- `test(integration): add one canonical prepare-snapshot-confirm-start-traceability lane`
- `test(runtime): add non-profile-compare lifecycle fingerprint continuity regression`
- `docs(protocol): freeze preview/start consistency invariants and failure reasons`

## 6. 设备执行安全审查

### 6.1 当前是否已有能力

有，而且已有多层安全线；但离“产品级可靠”还差一条关键正式门禁。

### 6.2 证据：文件路径、类、函数、测试、文档

- 代码证据
  - `modules/dispense-packaging/application/usecases/dispensing/PlanningUseCase.cpp`
    - `ResolveMachineBounds()` 读取 machine soft limits。
    - `AutoFitPrimitivesIntoMachineBounds()` 会自动平移进边界，仍越界则返回 `POSITION_OUT_OF_RANGE`。
    - `PrepareAuthorityPreview()` 在 process-path 前执行边界归位/拒绝。
  - `modules/dispense-packaging/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp`
    - 仍有基于 bounds 的 `POSITION_OUT_OF_RANGE` fail-closed 逻辑。
  - `modules/runtime-execution/application/usecases/dispensing/DispensingExecutionUseCase.Setup.cpp`
    - `ValidateExecutionPreconditions()` 会检查：
      - 连接状态
      - X/Y 轴 enabled
      - 已回零
      - 无 axis error / servo alarm / following error
      - `InterlockPolicy` 的 estop / door / pressure / temperature / voltage
    - `RefreshRuntimeParameters()` 用 `SafetyOutputGuard::Evaluate()` 约束 Test/Production 与工艺输出语义。
  - `modules/runtime-execution/application/services/motion/execution/MotionReadinessService.cpp`
    - `Evaluate()` 会挡 active job、coord system 未 settled、axis 未 settled。
  - `modules/runtime-execution/runtime/host/runtime/supervision/RuntimeSupervisionPortAdapter.cpp`
    - `BuildEffectiveInterlocksSnapshot()` 和 `ReadSnapshot()` 对外导出 `effective_interlocks`、`supervision`。
  - `apps/runtime-service/container/ApplicationContainer.Motion.cpp`
    - 明确注册了 `HardLimitMonitorService`。
    - 同文件中未注册 `SoftLimitMonitorService`。
  - `apps/runtime-service/container/ApplicationContainer.System.cpp`
    - `InitializeSystemUseCase` 只接收 `hard_limit_monitor_`，没有 soft-limit monitor 对应接线。
  - `modules/runtime-execution/application/usecases/system/InitializeSystemUseCase.cpp`
    - 系统初始化时会在 `request.start_hard_limit_monitoring` 条件下显式调用 `hard_limit_monitor_->Start()`。
    - 当前没有对 `SoftLimitMonitorService` 的对应启动逻辑。
  - `modules/runtime-execution/application/services/safety/SoftLimitValidator.cpp`
    - 存在 `ValidatePoint()`、`ValidateTrajectory()`，但当前仓库证据只看到定义和单测。
  - `modules/runtime-execution/runtime/host/services/motion/SoftLimitMonitorService.cpp`
    - 存在 `Start()`、`CheckSoftLimits()`、`HandleSoftLimitTrigger()`、`StopMotion()`，但当前仓库证据只看到定义和单测。
  - `modules/runtime-execution/application/usecases/motion/trajectory/ExecuteTrajectoryUseCase.cpp`
    - `ValidateTrajectory()` 当前只调用 `request.Validate()`，未见调用 `SoftLimitValidator`。
- 测试证据
  - `modules/dispense-packaging/tests/unit/application/usecases/dispensing/PlanningUseCaseExportPortTest.cpp`
    - 覆盖 plan-time `POSITION_OUT_OF_RANGE`。
  - `modules/runtime-execution/tests/unit/DispensingExecutionUseCaseInternalTest.cpp`
    - 覆盖 disconnected / dry-run / unhomed axis。
  - `modules/runtime-execution/tests/unit/dispensing/DispensingWorkflowUseCaseTest.cpp`
    - 覆盖 `safety_door_open` 时 `dxf.job.start` 被拒。
  - `modules/runtime-execution/tests/unit/domain/safety/InterlockPolicyTest.cpp`
  - `modules/runtime-execution/tests/unit/domain/safety/SafetyOutputGuardTest.cpp`
  - `modules/runtime-execution/tests/unit/domain/safety/SoftLimitValidatorTest.cpp`
  - `modules/runtime-execution/runtime/host/tests/unit/runtime/motion/HardLimitMonitorServiceTest.cpp`
  - `modules/runtime-execution/runtime/host/tests/unit/runtime/motion/SoftLimitMonitorServiceTest.cpp`
  - `tests/e2e/hardware-in-loop/run_real_dxf_machine_dryrun.py`
    - dry-run preflight 会阻断 `estop`、`door_open`、`limit_*`、`home_boundary_*`。
  - `tests/e2e/hardware-in-loop/run_real_dxf_machine_dryrun_negative_matrix.py`
    - 覆盖 `estop`、`door_open`、`home_boundary_x_active`、`home_boundary_y_active` 阻断。
  - `tests/integration/scenarios/first-layer/run_tcp_door_interlock.py`
    - 覆盖门禁触发后 `status.supervision` 和 `effective_interlocks` 可见，且 `dispenser.start`/`supply.open` 被阻断。

### 6.3 缺失能力

- `SoftLimitValidator` 未接入 `dxf.job.start` 前置校验，也未接入 `ExecuteTrajectoryUseCase::ValidateTrajectory()`。
- `SoftLimitMonitorService` 未接入 runtime-service motion 容器启动链。
- 当前产品安全主链更像：
  - plan-time bounds 防线
  - interlock/readiness 防线
  - HIL 脚本 preflight 防线
  - hard-limit runtime 防线
  - 但缺少 soft-limit 的产品内正式 start gate + in-run monitor

### 6.4 是否属于 P0 风险

是。

### 6.5 是否会导致“不该执行却执行”

会。

- 直接证据
  - `ValidateExecutionPreconditions()` 不检查 `soft_limit_positive/soft_limit_negative`。
  - `ExecuteTrajectoryUseCase::ValidateTrajectory()` 不调用 `SoftLimitValidator`。
  - `ApplicationContainer.Motion.cpp` 只注册 `HardLimitMonitorService`，没有注册 `SoftLimitMonitorService`。
- 推断
  - 即使 plan-time DXF 仍在 nominal bounds 内，只要实际机台状态已经处于 soft-limit 触发边缘，或者运行中进入 soft-limit 区域，当前 canonical product path 没有看到正式的 soft-limit fail-closed 机制。

### 6.6 最小补齐路径

- 不做双轨，不把安全继续留在脚本层。
- 最小补齐应是同一条 canonical safety chain：
  1. 在 `dxf.job.start` 前，对 execution trajectory 调用 `SoftLimitValidator::ValidateTrajectory()`；一旦越界，直接返回 `POSITION_OUT_OF_RANGE`，runtime start port 不得启动。
  2. 在 `apps/runtime-service/container/ApplicationContainer.Motion.cpp` 中注册并启动 `SoftLimitMonitorService`，与 `HardLimitMonitorService` 同级成为正式 host safety line。
  3. 若 soft-limit 触发，应通过现有 supervision/event 流输出明确原因，不能只留在脚本或日志里。

### 6.7 需要新增的测试

| 测试ID | 测什么 | 输入 | 期望输出 |
|---|---|---|---|
| EXEC-SAFE-01 | start 前软限位轨迹校验必须阻断 | 一个 execution trajectory 含点 `x=481.0`，而 machine `x_max=480.0` | `dxf.job.start` 返回 `POSITION_OUT_OF_RANGE`，runtime start port 不被调用 |
| EXEC-SAFE-02 | start 前已处于 soft-limit active 必须阻断 | 一个 motion state 报告 `soft_limit_negative=true` 的 mock runtime | `ValidateExecutionPreconditions()` 或等价 start gate 返回错误，job 不启动 |
| EXEC-SAFE-03 | runtime host 必须注册 soft-limit monitor | 启动 `ApplicationContainer`，提供 motion state/event/position control 端口 | `SoftLimitMonitorService` 被注册并启动；缺端口时给出明确告警 |
| EXEC-SAFE-04 | in-run 软限位触发必须停机并留痕 | 运行中的 mock motion state 在 Y 轴置 `soft_limit_negative=true` | 触发 stop/emergency stop，一次性发布 soft-limit event，事件包含 `axis/position/task_id` |
| EXEC-SAFE-05 | HIL negative 不得只靠脚本 preflight | 一个 canonical dry-run 场景，mock motion status 带 soft-limit active | `dxf.job.start` 在产品路径内被拒绝，而不是脚本提前退出后产品本身仍可启动 |

### 6.8 后续可交给 Codex 的具体任务

- `fix(runtime): wire SoftLimitValidator into dxf.job.start execution precheck`
- `fix(runtime-service): register and start SoftLimitMonitorService in ApplicationContainer.Motion`
- `test(runtime): add soft-limit-active and trajectory-out-of-range negative start tests`
- `test(hil): extend real_dxf_machine_dryrun_negative_matrix with soft-limit-active case`

## 7. 最小实现顺序

1. 先补 `soft limit` 正式安全链。
原因：这是唯一直接指向设备误执行风险的硬安全缺口。

2. 再补 runtime-owned `path_quality` gate。
原因：这是当前“低质量路径仍可能被生产化”的主要 owner contract 缺口。

3. 冻结 DXF 输入治理负向回归。
原因：当前语义已存在，先把它做成不会回归的 contract，再谈更细的前置预检。

4. 最后收口预览执行一致性的统一 regression lane。
原因：当前 start gate 已较强，剩余缺口更多是“证明深度”和“测试组织形态”。

## 8. 需要新增的测试清单

| ID | 所属项 | 类型 | 输入 | 期望输出 |
|---|---|---|---|---|
| DXF-GOV-01 | DXF 输入治理 | contract/unit | unsupported version DXF | `DXF_E_UNSUPPORTED_VERSION`，`production_ready=false` |
| DXF-GOV-02 | DXF 输入治理 | contract/unit | `$INSUNITS=0` 的最小生产 DXF | `preview_only`，`DXF_E_UNIT_REQUIRED_FOR_PRODUCTION`，`production_ready=false` |
| DXF-GOV-03 | DXF 输入治理 | contract/unit | 含 `3DFACE` 的 DXF | `DXF_E_3D_ENTITY_FOUND`，`classification=failed` |
| DXF-GOV-04 | DXF 输入治理 | contract/unit | 只含 `TEXT/MTEXT` 的 DXF | `DXF_E_NO_VALID_ENTITY`，`preview_ready=false` |
| DXF-GOV-06 | DXF 输入治理 | workflow/integration | `preview_only` import artifact + `dxf.job.start` | start 被拒绝，错误对齐 `import_summary` |
| PATH-Q-01 | 路径质量评价 | workflow/integration | `fragmentation_suspected=true` 的 synthetic path | `path_quality.blocking=true`，start 被拒绝 |
| PATH-Q-02 | 路径质量评价 | integration | `rect_diag.dxf` | `path_quality.verdict=pass`，start 允许 |
| PATH-Q-03 | 路径质量评价 | unit/integration | 微短段爆发 polyline | `path_quality.blocking=true` |
| PREVIEW-CONS-01 | 预览执行一致性 | integration | 同一 artifact 参数变更后二次 prepare | 旧 plan start 返回 `plan is stale` |
| PREVIEW-CONS-03 | 预览执行一致性 | integration/runtime | canonical `PROFILE_COMPARE` job 到 terminal | `dxf.job.traceability.verdict=passed` 且 `strict_one_to_one_proven=true` |
| EXEC-SAFE-01 | 设备执行安全 | unit/workflow | trajectory 点超 soft limit | `POSITION_OUT_OF_RANGE`，runtime start 不调用 |
| EXEC-SAFE-02 | 设备执行安全 | unit | motion state 已 soft-limit active | start gate 拒绝 job |
| EXEC-SAFE-03 | 设备执行安全 | host bootstrap | ApplicationContainer 启动 | `SoftLimitMonitorService` 已注册启动 |
| EXEC-SAFE-04 | 设备执行安全 | host unit | 运行中 soft-limit 触发 | stop + event publish + task snapshot |
| EXEC-SAFE-05 | 设备执行安全 | HIL negative | soft-limit active 的 canonical dry-run | 产品路径内阻断，不仅是脚本 preflight |

## 9. Codex 下一批任务拆分

1. `P0-SAFE-01`：把 `SoftLimitValidator` 接入 `dxf.job.start` 前置校验。
交付边界：只改 runtime start gate 和相邻单测；验收是超 soft-limit 轨迹无法启动。

2. `P0-SAFE-02`：在 `ApplicationContainer.Motion.cpp` 注册并启动 `SoftLimitMonitorService`。
交付边界：只改 runtime-service 容器 wiring、必要 host test；验收是 soft-limit 触发会停机并发事件。

3. `P0-PATH-01`：定义单一 canonical `path_quality` contract，并把现有 `preview_diagnostic_code/discontinuity_count/spacing` 收敛进去。
交付边界：只改 `dxf.plan.prepare` / `dxf.job.start` contract、workflow 组装和直接测试；不做大规模 planner 重构。

4. `P0-PATH-02`：新增 fragmented-path 和 micro-segment negative gate。
交付边界：补 synthetic fixtures + workflow/integration tests；验收是 low-quality path 明确被 blocking。

5. `P0-DXF-01`：冻结 DXF 输入治理负向 fixture matrix。
交付边界：只补 fixtures、`dxf_to_pb.py` contract tests、少量文档同步；不改主语义。

6. `P1-CONS-01`：收口一条统一 lifecycle regression lane。
交付边界：把 `prepare -> snapshot -> confirm -> start -> traceability` 串成正式回归；不改现有 gate 语义。
