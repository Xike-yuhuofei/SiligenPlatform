# NOISSUE Wave 2B Batch 2 Closeout

- 状态：Done
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-172135`
- 对应设计整合：`docs/process-model/plans/NOISSUE-wave2bD-integration-20260322-161904.md`

## 1. 总体裁决

1. `Wave 2B Batch 2 bridge = Done`
2. `Batch 3 进入裁决 = Go`
3. 本次仍处于“桥接态”，未执行任何 flip、未删除 compat/fallback、未改变 `build.ps1` / `test.ps1` 参数面、artifact 根或 `integration/reports` 根。

## 2. 子任务完成情况

### 2A root bridge

1. `tools/migration/validate_workspace_layout.py` 已收紧为单一 workspace-root/source-root 合同门禁。
2. 已验证：
   - `SILIGEN_SUPERBUILD_SOURCE_ROOT` 必须存在。
   - selector 默认值必须为 `.` 且解析到 workspace root。
   - `CMakeLists.txt`、`tests/CMakeLists.txt`、`tools/build/build-validation.ps1`、`packages/test-kit/src/test_kit/workspace_validation.py` 必须显式使用 selector/provenance。

### 2B runtime bridge

1. `packages/runtime-host/src/runtime/configuration/WorkspaceAssetPaths.h`
   - 新增 bridge 解析结果结构与 mode。
   - 保持 `config/machine_config.ini` 为只读 inbound compatibility alias。
   - 新增 canonical/compat 文件内容不一致时 `fail_fast=true`。
   - 补上“从非仓库根 cwd 传入相对路径”解析能力，修复 CLI 子目录合同失败。
2. `apps/control-runtime/main.cpp`
3. `apps/control-tcp-server/main.cpp`
4. `apps/control-cli/CommandHandlersInternal.h`
5. `packages/runtime-host/src/container/ApplicationContainer.cpp`
6. `packages/runtime-host/src/bootstrap/InfrastructureBindingsBuilder.cpp`
   - 上述入口和 runtime-host 装配层均已接入统一 bridge resolver，并在 compat alias 命中时保留可观测告警。
7. `packages/runtime-host/src/runtime/recipes/TemplateFileRepository.cpp`
   - 保持 canonical `templates/*.json` 为唯一写路径。
   - `data/recipes/templates.json` 仅保留只读 fallback。
8. 新增/更新单测：
   - `packages/runtime-host/tests/unit/runtime/configuration/WorkspaceAssetPathsTest.cpp`
   - `packages/runtime-host/tests/unit/runtime/recipes/TemplateFileRepositoryTest.cpp`
   - `packages/runtime-host/tests/CMakeLists.txt`

### 2C residual ledger split

1. `tools/migration/wave2b_residual_ledger.json` 已升级为 v2-only 账本。
2. `tools/migration/validate_wave2b_residuals.py` 已通过。
3. 当前统计：
   - `entry_count=9`
   - `runtime_build_debt=4`
   - `doc_boundary=3`
   - `historical_audit=2`
   - `issue_count=0`

## 3. 中途问题与修复

1. 首轮 `apps` 套件出现 2 个失败：
   - `control-cli-bootstrap-check-subdir`
   - `control-cli-dxf-preview-subdir`
2. 根因：
   - `ResolveConfigFilePathWithBridge(...)` 把任意相对路径都按 workspace-root 相对路径解释，导致在 `apps/control-cli` 子目录下传入 `..\\..\\config\\machine\\machine_config.ini` 时误判为不存在。
3. 修复：
   - 在 `WorkspaceAssetPaths.h` 中补上“cwd-relative 现存路径”优先解析。
   - 新增 `WorkspaceAssetPathsTest.ResolvesCurrentDirectoryRelativeCanonicalPath`。
   - 串行重编 `apps` 后，两个失败命令手工回放通过，`apps` 套件复跑转绿。

## 4. 实际执行命令

### 轻量校验

1. `python -m py_compile tools/migration/validate_wave2b_residuals.py tools/migration/validate_workspace_layout.py packages/test-kit/src/test_kit/runner.py packages/test-kit/src/test_kit/workspace_validation.py`
2. `python tools/migration/validate_wave2b_residuals.py`
3. `python tools/migration/validate_workspace_layout.py`
4. `python -m unittest apps/control-cli/tests/unit/test_cli_cleanup_contract.py`

### 构建

1. `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile Local -Suite packages`
2. `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile Local -Suite apps`
3. `C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build\bin\Debug\siligen_runtime_host_unit_tests.exe`

### 根级 suites

1. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite packages -ReportDir integration\reports\verify\wave2b-batch2\packages -FailOnKnownFailure`
2. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite apps -ReportDir integration\reports\verify\wave2b-batch2\apps -FailOnKnownFailure`
3. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite integration -ReportDir integration\reports\verify\wave2b-batch2\integration -FailOnKnownFailure`
4. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite protocol-compatibility -ReportDir integration\reports\verify\wave2b-batch2\protocol -FailOnKnownFailure`
5. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite simulation -ReportDir integration\reports\verify\wave2b-batch2\simulation -FailOnKnownFailure`

### 失败命令回放

1. `siligen_cli.exe bootstrap-check --config ..\..\config\machine\machine_config.ini`（cwd=`apps/control-cli`）
2. `siligen_cli.exe dxf-preview --file ..\..\examples\dxf\rect_diag.dxf --preview-max-points 200 --json --config ..\..\config\machine\machine_config.ini`（cwd=`apps/control-cli`）

## 5. 结果摘要

1. `packages` suite：`passed=8 failed=0`
   - 报告：`integration/reports/verify/wave2b-batch2/packages/workspace-validation.{json,md}`
2. `apps` suite：`passed=18 failed=0`
   - 报告：`integration/reports/verify/wave2b-batch2/apps/workspace-validation.{json,md}`
3. `integration` suite：`passed=1 failed=0`
   - 报告：`integration/reports/verify/wave2b-batch2/integration/workspace-validation.{json,md}`
4. `protocol-compatibility` suite：`passed=1 failed=0`
   - 报告：`integration/reports/verify/wave2b-batch2/protocol/workspace-validation.{json,md}`
5. `simulation` suite：`passed=5 failed=0`
   - 报告：`integration/reports/verify/wave2b-batch2/simulation/workspace-validation.{json,md}`
6. `runtime-host` 单测可执行物：`24/24 passed`

## 6. Batch 3 裁决

1. `Batch 2` 的退出条件已满足：
   - 默认值仍未翻转。
   - 关键入口已切到统一 bridge 机制。
   - 根级 suites 全绿。
2. `Batch 3 = Go`。
3. 前提仍然成立：
   - 必须严格串行为 `3A -> 3B -> 3C`。
   - 不能把 bridge 完成误解为可以直接删桥。
   - 任一 flip 失败必须按合同单元立即回滚。

## 7. 非阻断注意项

1. app 入口层与 runtime-host 装配层当前都做了一次 config bridge 解析，语义一致但可能产生重复告警；这不是 Batch 2 阻断项。
2. `logs/*` 等非本批次目标路径仍未纳入统一策略；按设计冻结约束，本轮未处理。

