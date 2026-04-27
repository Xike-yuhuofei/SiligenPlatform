# NOISSUE Wave 2B Batch 3B Closeout

- 状态：Done
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-180220`
- 对应计划：`docs/process-model/plans/NOISSUE-wave2b-batch3b-runtime-asset-cutover-plan-20260322-180220.md`

## 1. 总体裁决

1. `Wave 2B Batch 3B = Done`
2. `3B runtime asset-path flip = Go`
3. `Batch 3C 进入裁决 = Go`
4. 本批次未删除 `config/machine_config.ini` compatibility alias，也未删除 `templates.json` fallback；删除动作仍留给后续批次。

## 2. 本批次完成内容

### 3B-1 Config bridge owner 收口

1. `packages/runtime-host/src/ContainerBootstrap.cpp`
   - `BuildContainer(...)` 成为 config bridge 的唯一解析 owner。
   - 统一输出 compat alias warning，并把 resolved canonical path 向下游透传。
   - 新增可选 `resolved_config_file_path` 输出参数，供 app 入口读取最终生效路径而不重复 resolve。
2. `apps/control-runtime/main.cpp`
   - 删除入口层重复 resolve/warning。
   - 子目录相对 canonical/compat alias 均通过 `BuildContainer(...)` 进入统一装配链。
3. `apps/control-tcp-server/main.cpp`
   - 删除入口层重复 resolve/warning。
   - `TcpServerHostOptions` 改为使用 `BuildContainer(...)` 返回的 resolved canonical path。
4. `packages/runtime-host/src/container/ApplicationContainer.cpp`
   - 移除构造期重复 resolve/warning。
5. `packages/runtime-host/src/bootstrap/InfrastructureBindingsBuilder.cpp`
   - 改为消费已解析 config path，不再二次桥接解析。

### 3B-2 Template legacy fallback 收紧

1. `packages/runtime-host/src/runtime/recipes/TemplateFileRepository.cpp`
   - 增加 canonical/legacy 统一 inventory。
   - same id 或 same name 且内容不一致时，`get/list/save` 统一 fail-fast。
   - 等价内容允许并存，canonical 保持优先，legacy 仅补漏。
   - `save` 继续只写 `templates/*.json`。
2. `packages/runtime-host/tests/unit/runtime/recipes/TemplateFileRepositoryTest.cpp`
   - 新增冲突 fail-fast 与等价并存覆盖。

### 3B-3 静态门禁与回归扩展

1. `tools/migration/validate_runtime_asset_cutover.py`
   - 新增 runtime asset cutover 静态 gate。
2. `packages/test-kit/src/test_kit/workspace_validation.py`
   - packages suite 接入 `runtime-asset-cutover`。
   - apps suite 新增：
     - `control-runtime-subdir-canonical-config`
     - `control-runtime-compat-warning-once`
     - `control-tcp-server-subdir-canonical-config`
     - `control-tcp-server-compat-warning-once`
     - `control-cli-bootstrap-check-compat-warning-once`
   - compat alias 用例均验证“恰好 1 次告警”。
3. `packages/runtime-host/tests/unit/runtime/configuration/WorkspaceAssetPathsTest.cpp`
   - 新增 marker 不完整与 workspace root 缺失负向用例。

## 3. 实际执行命令

### 静态校验

1. `python -m py_compile tools/migration/validate_runtime_asset_cutover.py packages/test-kit/src/test_kit/workspace_validation.py`
2. `python tools/migration/validate_runtime_asset_cutover.py`
3. `python tools/migration/validate_workspace_layout.py`

### 构建与单测

1. `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile Local -Suite packages`
2. `C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build\bin\Debug\siligen_runtime_host_unit_tests.exe`
3. `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile CI -Suite apps`

### 根级 suites

1. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite apps -ReportDir integration\reports\verify\wave2b-batch3b\apps -FailOnKnownFailure`
2. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite packages -ReportDir integration\reports\verify\wave2b-batch3b\packages -FailOnKnownFailure`
3. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite integration -ReportDir integration\reports\verify\wave2b-batch3b\integration -FailOnKnownFailure`
4. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite protocol-compatibility -ReportDir integration\reports\verify\wave2b-batch3b\protocol -FailOnKnownFailure`
5. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite simulation -ReportDir integration\reports\verify\wave2b-batch3b\simulation -FailOnKnownFailure`

## 4. 关键证据

1. `packages` suite 的 `runtime-asset-cutover` 结果：
   - `config/machine_config.ini` 命中仅剩 `packages/runtime-host/src/runtime/configuration/WorkspaceAssetPaths.h`
   - `templates.json` 命中仅剩 `packages/runtime-host/src/runtime/recipes/TemplateFileRepository.cpp` 与其 test
   - `issue_count=0`
2. `apps` suite 新增用例全部通过：
   - `control-runtime-subdir-canonical-config`
   - `control-runtime-compat-warning-once`
   - `control-tcp-server-subdir-canonical-config`
   - `control-tcp-server-compat-warning-once`
   - `control-cli-bootstrap-check-compat-warning-once`
3. compat alias 告警次数证据：
   - `control-runtime-compat-warning-once` 输出仅 1 条 `[runtime-host] [WARNING] 命中兼容配置桥接`
   - `control-tcp-server-compat-warning-once` 输出仅 1 条 `[runtime-host] [WARNING] 命中兼容配置桥接`
   - `control-cli-bootstrap-check-compat-warning-once` 输出仅 1 条 `[WARNING] CLI 命中兼容配置桥接`
4. runtime-host unit：
   - `30 tests from 4 test suites`
   - `PASSED 30 tests`

## 5. 结果摘要

1. `apps` suite：`passed=23 failed=0 known_failure=0 skipped=0`
2. `packages` suite：`passed=9 failed=0 known_failure=0 skipped=0`
3. `integration` suite：`passed=1 failed=0 known_failure=0 skipped=0`
4. `protocol-compatibility` suite：`passed=1 failed=0 known_failure=0 skipped=0`
5. `simulation` suite：`passed=5 failed=0 known_failure=0 skipped=0`

## 6. 非阻断记录

1. `config/machine_config.ini` 仍保持 warning-only inbound compatibility alias；runtime hard-fail 未在本批次启用。
2. `templates.json` 仍保留只读 fallback；本批次只补冲突 fail-fast，不执行删除。
3. `logs/*` 与其他 runtime 非 canonical 路径统一策略仍未纳入本批次。
4. 构建期间仍有 protobuf/abseil 链接告警，但未改变 `3B` 结论。

## 7. 3C 裁决

1. `3B` 的退出条件已满足：
   - config bridge owner 已收口到 `BuildContainer(...)`
   - compat alias 保持 warning-only 且告警次数受控
   - template canonical/legacy 冲突已 fail-fast
   - 静态 gate 与根级 suites 全绿
2. `3C = Go`
3. 后续仍必须保持 `Batch 3` 严格串行：
   - 下一步只能进入 `3C` residual payoff / payoff gate
   - 在 `3C` closeout 前，不得提前推进 bridge 删除类动作
