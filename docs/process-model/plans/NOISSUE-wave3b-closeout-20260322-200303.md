# NOISSUE Wave 3B Closeout

- 状态：Done
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-200303`
- 上游收口：`docs/process-model/plans/NOISSUE-wave3a-closeout-20260322-194649.md`
- 关联计划：
  - `docs/process-model/plans/NOISSUE-wave3b-scope-20260322-200303.md`
  - `docs/process-model/plans/NOISSUE-wave3b-arch-plan-20260322-200303.md`
  - `docs/process-model/plans/NOISSUE-wave3b-test-plan-20260322-200303.md`
- 关联验证目录：`integration/reports/verify/wave3b-closeout/`

## 1. 总体裁决

1. `Wave 3B = Done`
2. `Wave 3B closeout = Done`
3. `Wave 3C planning = Go`
4. `Wave 3C implementation = No-Go`

## 2. 本阶段完成内容

### 2.1 gateway `config_path` 合同面收紧

1. `packages/transport-gateway/include/siligen/gateway/tcp/tcp_server_host.h`
   - `TcpServerHostOptions` 只保留 `port`，不再暴露 `config_path`。
2. `packages/transport-gateway/src/tcp/TcpCommandDispatcher.h/.cpp`
   - 删除构造函数中的 `config_path` 参数。
   - 删除 `config_path_` 成员与 gateway 内部 canonical fallback。
3. `packages/transport-gateway/src/wiring/TcpServerHost.cpp`
   - gateway 只通过 `IConfigurationPort` 消费配置事实。
4. `apps/control-tcp-server/main.cpp`
   - 继续在 app/runtime-host 边界解析 `resolved_config_path`。
   - 不再把解析后的路径透传到 gateway 公共接口。

### 2.2 HMI 本地 preview residual 删除

1. 删除：
   - `apps/hmi-app/src/hmi_client/integrations/dxf_pipeline/__init__.py`
   - `apps/hmi-app/src/hmi_client/integrations/dxf_pipeline/preview_client.py`
   - `apps/hmi-app/tests/unit/test_preview_client.py`
2. `packages/engineering-contracts/tests/test_engineering_contracts.py`
   - preview fixture consumer 改为 canonical `PreviewArtifact`。
   - 不再依赖 HMI 本地 preview dataclass。
3. `apps/hmi-app/DEPENDENCY_AUDIT.md`
   - HMI 运行时 preview gate 现在只以 gateway snapshot/confirm 为主链路。

### 2.3 文档与 current-fact 口径统一

1. `packages/transport-gateway/README.md`
2. `packages/engineering-contracts/README.md`
3. `docs/architecture/dxf-pipeline-strangler-progress.md`
4. `docs/architecture/dxf-pipeline-final-cutover.md`
5. `docs/architecture/redundancy-candidate-audit.md`

统一结论：

1. gateway 不再是 `config_path` owner
2. HMI 不再依赖本地 preview integration shim
3. preview artifact producer / contract owner / runtime preview gate owner 已分层清晰

## 3. 实际执行命令

### 3.1 静态与单元验证

1. `python packages/transport-gateway/tests/test_transport_gateway_compatibility.py`
2. `python packages/engineering-contracts/tests/test_engineering_contracts.py`
3. `powershell -NoProfile -ExecutionPolicy Bypass -File apps/hmi-app/scripts/test.ps1`

### 3.2 构建与根级 suites

1. `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile CI -Suite apps`
2. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite packages -ReportDir integration\reports\verify\wave3b-closeout\packages -FailOnKnownFailure`
3. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite apps -ReportDir integration\reports\verify\wave3b-closeout\apps -FailOnKnownFailure`
4. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite protocol-compatibility -ReportDir integration\reports\verify\wave3b-closeout\protocol -FailOnKnownFailure`

## 4. 关键证据

1. 实施相关 diff 统计：
   - `13 files changed, 75 insertions(+), 64 deletions(-)`
2. `integration/reports/verify/wave3b-closeout/packages/workspace-validation.md`
   - `passed=10 failed=0 known_failure=0 skipped=0`
   - `transport-gateway compatibility checks passed`
   - `engineering-contracts` 兼容性测试继续通过
3. `integration/reports/verify/wave3b-closeout/apps/workspace-validation.md`
   - `passed=23 failed=0 known_failure=0 skipped=0`
   - `hmi-app-unit` 为 `Ran 65 tests ... OK`
   - `control-tcp-server-subdir-canonical-config` 与 `control-tcp-server-compat-alias-rejected` 继续通过
4. `integration/reports/verify/wave3b-closeout/protocol/workspace-validation.md`
   - `passed=1 failed=0 known_failure=0 skipped=0`
   - `protocol compatibility suite passed`
5. scoped 负向搜索：
   - `rg -n "DxfPipelinePreviewClient|DxfPreviewResult|integrations/dxf_pipeline/preview_client" apps/hmi-app/src packages/transport-gateway apps/control-tcp-server docs/architecture packages/engineering-contracts ...`
   - 当前结果为 `0` 命中
6. scoped 负向搜索：
   - `rg -n "config_path_" packages/transport-gateway/src packages/transport-gateway/include apps/control-tcp-server ...`
   - 当前结果为 `0` 命中

## 5. 阶段结论

1. `Wave 3B` 已完成 gateway 配置合同收窄，且没有破坏 `control-tcp-server` 的 canonical 配置路径合同。
2. `Wave 3B` 已删除 HMI 本地 preview residual，preview contract gate 改为 canonical consumer，不再借 HMI 本地 shim。
3. 当前可以进入 `Wave 3C planning`，但不能直接进入 `Wave 3C implementation`；后续 residual 仍需新的 scope / arch / test 工件先行锁定。

## 6. 非阻断记录

1. 构建期间 protobuf/abseil 的既有链接警告仍存在；本批次未改变其结论。
2. 当前工作树仍明显不干净，但不影响 `Wave 3B closeout` 的 authorative 证据闭环。
3. `control-cli` 的其余 `cwd`/path 旁路、以及 `dxf-pipeline` 仓外兼容窗口，仍保留给后续阶段处理。
