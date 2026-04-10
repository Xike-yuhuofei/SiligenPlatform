# NOISSUE Wave 5A Release Scope

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-234806`
- base 分支：`main`
- 对比基线：`origin/main...HEAD`

## 1. 发布目标

1. 以单 PR 方式完成当前分支发布收口。
2. 以最小门槛完成关键链路验证并补齐 premerge/qa/release 证据。
3. 不新增业务范围，按当前已存在变更集收口。

## 2. 提交范围（按 commit diff）

`origin/main...HEAD` 当前包含 32 个文件差异，核心分组如下：

1. HMI 预览与状态同步
   - `apps/hmi-app/src/hmi_client/client/protocol.py`
   - `apps/hmi-app/src/hmi_client/ui/main_window.py`
   - `apps/hmi-app/tests/unit/test_protocol_preview_gate_contract.py`
2. runtime/core 运动与安全链路
   - `packages/process-runtime-core/src/application/usecases/dispensing/DispensingWorkflowUseCase.cpp`
   - `packages/runtime-host/src/security/InterlockMonitor.cpp`
   - `packages/runtime-host/src/runtime/configuration/InterlockConfigResolver.cpp`
   - 相关单测与兼容验证
3. transport 与文档证据
   - `packages/transport-gateway/src/tcp/TcpCommandDispatcher.cpp`
   - `packages/transport-gateway/tests/test_transport_gateway_compatibility.py`
   - `docs/architecture/*`、`docs/process-model/*`、`docs/troubleshooting/canonical-runbook.md`

## 3. 非提交范围说明

1. 当前工作区存在大量未提交改动与未跟踪文件，不自动纳入本次发布提交。
2. `integration/reports/**` 作为执行证据目录使用，不作为发布提交物。
3. `Wave 4E` 外部观察结论已为 `Go`，本阶段只做发布门禁复验，不重开迁移整改。

## 4. 门禁口径

1. premerge：必须先 `git fetch origin main`，再基于 `origin/main...HEAD` 输出结构化审查。
2. qa：按最小门槛串行执行：
   - `verify-engineering-data-cli.ps1`
   - `run-external-migration-observation.ps1`（observation recheck）
   - `test.ps1 -Profile CI -Suite apps -FailOnKnownFailure`
3. push/pr：门禁通过后执行，失败则在 release 摘要中标注 `BLOCKED`。
