# NOISSUE Wave 3A Closeout

- 状态：Done
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-194649`
- 上游收口：`docs/process-model/plans/NOISSUE-wave2b-closeout-20260322-191927.md`
- 关联计划：
  - `docs/process-model/plans/NOISSUE-wave3a-scope-20260322-193040.md`
  - `docs/process-model/plans/NOISSUE-wave3a-arch-plan-20260322-193040.md`
  - `docs/process-model/plans/NOISSUE-wave3a-test-plan-20260322-193040.md`
- 关联验证目录：`integration/reports/verify/wave3a-closeout/`

## 1. 总体裁决

1. `Wave 3A = Done`
2. `Wave 3A closeout = Done`
3. `Wave 3B planning = Go`
4. `Wave 3B implementation = No-Go`

## 2. 本阶段实际完成内容

### 2.1 gateway canonical target 收口

1. `packages/transport-gateway/CMakeLists.txt`
   - 删除 3 个 legacy alias target：
     - `siligen_control_gateway_tcp_adapter`
     - `siligen_tcp_adapter`
     - `siligen_control_gateway`
   - 终态只保留：
     - `siligen_transport_gateway`
     - `siligen_transport_gateway_protocol`
2. `packages/transport-gateway/tests/test_transport_gateway_compatibility.py`
   - 测试口径改为“canonical target 必须存在，legacy alias 不得回流”。
3. `packages/transport-gateway/README.md`
   - README 与实现终态统一到 canonical-only 口径。

### 2.2 app shell / HMI launch contract 防回流

1. `apps/control-tcp-server/README.md`
   - 明确 app shell 只消费公开 builder/host 边界。
2. `apps/hmi-app/tests/unit/test_gateway_launch.py`
   - 新增 3 组 contract 防回流覆盖：
     - 显式 `SILIGEN_GATEWAY_LAUNCH_SPEC` 优先
     - app 默认 `config/gateway-launch.json` 优先
     - 缺少 `executable` 的坏 spec 必须失败
3. 本阶段没有扩展新的 gateway fallback 来源，也没有引入 app 内部直连 gateway 实现的修改。

### 2.3 文档 residual 统一

1. `docs/architecture/canonical-paths.md`
2. `docs/architecture/build-and-test.md`
3. `docs/architecture/directory-responsibilities.md`
4. `docs/architecture/legacy-exit-checks.md`
5. `packages/process-runtime-core/README.md`
6. `packages/transport-gateway/tests/README.md`

以上文档已统一到同一结论：

1. gateway/tcp legacy alias 已删除
2. 相关测试职责是防回流，而不是继续证明兼容壳活跃
3. `packages/transport-gateway` 的“兼容壳”仅剩历史审计语义，不再是当前活跃设计

## 3. 执行过的验证

### 3.1 静态与单项验证

1. `python packages/transport-gateway/tests/test_transport_gateway_compatibility.py`
2. `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile CI -Suite apps`

### 3.2 根级 suites

1. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite packages -ReportDir integration\reports\verify\wave3a-closeout\packages -FailOnKnownFailure`
2. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite apps -ReportDir integration\reports\verify\wave3a-closeout\apps -FailOnKnownFailure`
3. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite protocol-compatibility -ReportDir integration\reports\verify\wave3a-closeout\protocol -FailOnKnownFailure`

## 4. 关键证据

1. `git diff --stat`（本阶段相关 11 文件）
   - `11 files changed, 114 insertions(+), 29 deletions(-)`
2. `integration/reports/verify/wave3a-closeout/packages/workspace-validation.md`
   - `passed=10 failed=0 known_failure=0 skipped=0`
   - `transport-gateway compatibility checks passed`
   - `workspace-layout-boundary`、`runtime-asset-cutover`、`wave2b-residual-ledger`、`wave2b-residual-payoff` 继续通过
3. `integration/reports/verify/wave3a-closeout/apps/workspace-validation.md`
   - `passed=23 failed=0 known_failure=0 skipped=0`
   - `hmi-app-unit` 为 `Ran 68 tests ... OK`
   - `control-tcp-server-subdir-canonical-config` 继续通过
   - `control-tcp-server-compat-alias-rejected` 继续通过
4. `integration/reports/verify/wave3a-closeout/protocol/workspace-validation.md`
   - `passed=1 failed=0 known_failure=0 skipped=0`
   - `protocol compatibility suite passed`
5. `build.ps1 -Profile CI -Suite apps`
   - 构建通过，未因 gateway alias 删除引入新的 link failure

## 5. 阶段结论

1. `Wave 2B` 的 canonical workspace / runtime asset / residual zero-state 前提保持成立。
2. `Wave 3A` 已把 `TCP/Gateway boundary first` 的最小收口完成，并补上了 app shell 与 HMI launch contract 的防回流证据。
3. 当前可以进入 `Wave 3B planning`，但不能直接进入 `Wave 3B implementation`，因为后续 residual 与 owner 切分仍需新的 scope / arch / test 工件先行锁定。

## 6. 非阻断记录

1. 构建期间仍存在 protobuf/abseil 的既有链接警告；本批次未改变该结论。
2. 当前工作树仍明显不干净，但这不影响 `Wave 3A closeout`，只意味着此刻不适合再发起新的独立 QA 工作流。
3. 以下 residual 仍保留给后续阶段处理，不属于 `Wave 3A` 阻断项：
   - `packages/transport-gateway` 的 `config_path` 透传合同仍偏宽
   - `apps/hmi-app/src/hmi_client/integrations/dxf_pipeline/preview_client.py` 仍是 HMI 本地 DXF preview residual
