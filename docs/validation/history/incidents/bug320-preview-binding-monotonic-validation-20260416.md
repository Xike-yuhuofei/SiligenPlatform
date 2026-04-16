# BUG-320 Preview Binding Monotonic Validation 2026-04-16

- 任务类型：`incident`
- 分支：`fix/path/BUG-320-preview-binding-monotonic`
- 验证日期：`2026-04-16`
- 验证目标：收口 `Demo.dxf` real-dxf preview binding 非单调问题，并确认 canonical `preview-only` 验收通过。

## 根因结论

- 下游 authority 真值必须冻结为 `shaped_path`；`process_path` 只能保留为诊断对照，不能重新抬升为下游 authority。
- authority trigger layout 在 open span / explicit boundary / revisited endpoint 组合场景下，若继续保留 open span 末端 anchor，会把后续 explicit boundary 的真实起点绑定挤到更早已访问过的位置，形成非单调 binding。
- trajectory trigger marker 在 sequential segment 上若接受约等于端点的投影，并对开放路径允许 wraparound / backward fallback，会把 authority trigger 重新绑回先前轨迹点，放大上述非单调问题。
- 当 `binding_monotonic=false` 仅作为软信号保留时，`Demo.dxf` preview 主链会继续下探，最终在 canonical 验收中暴露为 `authority trigger binding non-monotonic`。

## 修复内容

- `PlanningUseCase` 下游 authority 真值收口到 `shaped_path`。
- `PlanningAssemblyExecutionBinding` 将 `binding_monotonic=false` 升级为硬失败门，直接阻断错误链路继续下探。
- `AuthorityTriggerLayoutPlanner` 改为跨 component 全局按 `start_distance_mm` 发射，并新增：
  - 当 open span 末端 anchor 会在后续 explicit-boundary span 起点之后再次被经过时，抑制该末端 anchor。
  - 保留闭环 carrier 的受控 wraparound，避免误伤 closed-loop phase-shift 场景。
- `TrajectoryTriggerUtils` 新增：
  - sequential segment 只接受 interior projection，不接受约等于端点的投影；
  - batch apply 优先前向 interior projection；
  - 开放路径禁 wraparound / backward fallback。
- `PlanningArtifactExportRequest` / `PlanningAssemblyExecutionPackaging` 补齐 execution trigger monotonic 诊断元数据，确保 contract 侧能直接断言本轮修复语义。

## 离线验证

- 已通过回归：
  - `D:/Projects/SiligenSuite/build/ca/bin/Debug/siligen_dispense_packaging_unit_tests.exe`
  - `D:/Projects/SiligenSuite/build/ca/bin/Debug/siligen_dispense_packaging_workflow_seam_tests.exe`
  - `D:/Projects/SiligenSuite/build/ca/bin/Debug/siligen_dispense_packaging_shared_adjacent_regression_tests.exe`

- 本轮关键测试覆盖：
  - `AuthorityTriggerLayoutPlannerTest.SuppressesOpenEndAnchorWhenFutureExplicitBoundaryReachesItAfterStart`
  - `WorkflowPlanningAssemblyOperationsContractTest.BuildExecutionArtifactsFromAuthorityKeepsOverlappedReturnBoundaryMonotonic`
  - `WorkflowPlanningAssemblyOperationsContractTest.AssemblePlanningArtifactsBindsAuthorityAcrossClosedLoopPhaseShiftedExecution`
  - `TrajectoryTriggerUtilsTest.ApplyTriggerMarkersByPositionPrefersForwardSegmentProjectionOverLaterExactRevisit`
  - `TrajectoryTriggerUtilsTest.ApplyTriggerMarkersByPositionAllowsClosedLoopPhaseShiftWraparound`
  - `PlanningUseCaseExportPortTest.WorkflowUsesShapedPathAsDownstreamAuthorityPath`

## Canonical Preview-Only 验收

- 验收命令：

```powershell
python D:/Projects/SiligenSuite/tests/e2e/hardware-in-loop/run_real_dxf_preview_snapshot.py --gateway-exe D:/Projects/SiligenSuite/build/ca/bin/Debug/siligen_runtime_gateway.exe --config-mode mock --dxf-file D:/Projects/SiligenSuite/uploads/dxf/archive/Demo.dxf --recipe-id recipe-7d1b00f4-6a99 --version-id version-fea9ce29-f963
```

- 约束：
  - 本轮只做 `preview-only` 验收；
  - 不把本次通过结论外推为 production validation 已通过。

- 结果：
  - 报告：`D:/Projects/SiligenSuite/tests/reports/adhoc/real-dxf-preview-snapshot-canonical/20260416-225035/preview-verdict.json`
  - `verdict=passed`
  - `online_ready=true`
  - `preview_confirmed=true`
  - `geometry_semantics_match=true`
  - `order_semantics_match=true`
  - `dispense_motion_semantics_match=true`

## 关键证据

- preview verdict：
  - `D:/Projects/SiligenSuite/tests/reports/adhoc/real-dxf-preview-snapshot-canonical/20260416-225035/preview-verdict.json`
- 失败前对照：
  - `D:/Projects/SiligenSuite/tests/reports/adhoc/real-dxf-preview-snapshot-canonical/20260416-223804/real-dxf-preview-snapshot.json`
  - 失败原因为 `authority trigger binding non-monotonic`
- 关键日志：
  - `D:/Projects/SiligenSuite/logs/tcp_server.log`

## 当前结论

- `Demo.dxf` 的 canonical `preview-only` 主链已通过，authority trigger binding 非单调问题已被当前硬门与布局/marker 双侧修复收口。
- 当前结论只覆盖 `Demo.dxf` 和 `preview-only` 验收，不覆盖 production validation，也未扩面到更多真实 DXF 样本。
