# QA Regression Report

## 1. 环境信息（分支、commit、执行时间）
- 分支：feat/hmi/NOISSUE-dispense-trajectory-preview-v2
- Commit：36677a6d
- 执行时间：2026-03-22 03:44:03 +08:00
- 测试计划：docs/process-model/plans/NOISSUE-test-plan-20260321-221824.md
- 工作树状态：git status --porcelain 检测到 22 条变更（非干净工作树）

## 2. 执行命令与退出码
1. powershell -NoProfile -ExecutionPolicy Bypass -File apps/hmi-app/scripts/test.ps1 -> 0
2. python packages/transport-gateway/tests/test_transport_gateway_compatibility.py -> 0
3. out/build-runtime-preview/bin/siligen_unit_tests.exe --gtest_filter="DispensingExecutionUseCaseProgressTest.*:DispensingWorkflowUseCaseTest.*:MotionMonitoringUseCaseTest.*:RedundancyGovernanceUseCasesTest.*" -> 0
4. out/build-runtime-preview/bin/siligen_unit_tests.exe --gtest_filter="JsonRedundancyRepositoryAdapterTest.*" -> 0
5. .\test.ps1 -Profile CI -Suite apps -FailOnKnownFailure -> 0

## 3. 测试范围
- HMI 预览门禁与协议契约回归（含新增 dxf_preview_snapshot_with_error_details）。
- transport-gateway 预览快照命令契约回归（含 max_polyline_points 上限夹断断言）。
- runtime 关键状态机回归（DispensingWorkflowUseCase、执行进度、监控、redundancy 定向）。
- 根级 apps 套件 CI 轮询回归。

## 4. 失败项与根因
- 测试执行层面：未发现失败用例。
- 流程门禁层面：存在阻断项。
- 问题：工作树非干净（并行改动与本需求改动共存）。
- 根因：当前分支尚未完成按需求范围拆分提交，无法形成最小可审计变更集。

## 5. 修复项与验证证据（包含复测命令）
1. HMI 修复：重同步失败可判定不可恢复时执行一次性降级，停止 2 秒轮询重试，提示重新生成并确认。
- 代码：apps/hmi-app/src/hmi_client/ui/main_window.py
- 协议支持：apps/hmi-app/src/hmi_client/client/protocol.py
- 单测：apps/hmi-app/tests/unit/test_protocol_preview_gate_contract.py
- 复测证据：命令 #1 通过（65 tests, OK）。
2. Gateway 修复验证：补充 dxf.preview.snapshot 兼容回退告警与 max_polyline_points 夹断实现的兼容断言。
- 测试：packages/transport-gateway/tests/test_transport_gateway_compatibility.py
- 复测证据：命令 #2 通过。
3. Runtime 预览状态一致性验证：
- 代码：packages/process-runtime-core/src/application/usecases/dispensing/DispensingWorkflowUseCase.cpp
- 测试：packages/process-runtime-core/tests/unit/dispensing/DispensingWorkflowUseCaseTest.cpp
- 复测证据：命令 #3 中 DispensingWorkflowUseCaseTest.* 5/5 通过。

## 6. 未修复项与风险接受结论
- 未修复项：范围污染风险（本需求无关改动仍在同一工作树中）。
- 风险接受结论：不接受。该项会直接影响 PR 可审查性与回滚边界，需先完成变更拆分。

## 7. 发布建议（Go / No-Go）
- 结论：No-Go
- 原因：虽然测试全绿，但工作树未收敛为最小需求范围，未满足发布前可审计变更集门禁。
- 放行条件：完成按功能拆分/提交后，复跑本报告命令并保持全绿。
