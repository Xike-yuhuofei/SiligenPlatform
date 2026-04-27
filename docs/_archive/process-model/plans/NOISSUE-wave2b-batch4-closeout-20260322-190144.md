# NOISSUE Wave 2B Batch 4 Closeout

- 状态：Done
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-190144`

## 1. 总体裁决

1. `Wave 2B Batch 4 = Done`
2. `Wave 2B bridge/fallback removal = Done`
3. `Wave 2B residual ledger terminal zero state = Done`
4. `Wave 2B = Done`

## 2. 本批次完成内容

### 4A root source-root bridge 删除

1. `cmake/workspace-layout.env`
   - 删除 `SILIGEN_SUPERBUILD_SOURCE_ROOT`。
2. `CMakeLists.txt`、`tests/CMakeLists.txt`
   - 删除 selector 解析与缓存。
   - `CMAKE_SOURCE_DIR` / tests candidate 现在直接被固定为唯一 canonical workspace root。
3. `tools/build/build-validation.ps1`
   - 删除 selector 输出与 legacy relation 输出。
   - 保留 `control-apps` build cache 的 `CMAKE_HOME_DIRECTORY == repo root` provenance hard-gate。
4. `packages/test-kit/src/test_kit/workspace_validation.py`
   - metadata 收敛为 `workspace_root` / `control_apps_cmake_home_directory`。
5. `tools/migration/validate_workspace_layout.py`
   - 删除 selector 检查，改为 root pinning / provenance / wiring 终态检查。

### 4B runtime compat alias / fallback 删除

1. `packages/runtime-host/src/runtime/configuration/WorkspaceAssetPaths.h`
   - 删除 `config/machine_config.ini` compat 解析与 warning-only bridge 行为。
   - 若现场仍残留 root alias 文件，则 canonical 请求也会 fail-fast。
2. `packages/runtime-host/src/ContainerBootstrap.cpp`
   - 删除 runtime-host compat warning，改为统一解析失败报错。
3. `apps/control-cli/CommandHandlersInternal.h`
   - 删除 CLI compat warning，显式旧路径改为直接失败。
4. `packages/runtime-host/src/runtime/recipes/TemplateFileRepository.cpp`
   - 删除 `templates.json` merge/fallback。
   - 若现场残留 legacy 单文件，`list/get/save` 全部 fail-fast。
5. `packages/runtime-host/CMakeLists.txt`
   - 删除 `siligen_device_hal_persistence` / `siligen_persistence` alias。
6. `CMakeLists.txt`
   - 删除 build/install 对 `config/machine_config.ini` bridge 的 staging。
7. `config/machine_config.ini`
   - 从仓库删除 root alias 实体文件。

### 4B 测试与 gate 同步

1. `packages/runtime-host/tests/unit/runtime/configuration/WorkspaceAssetPathsTest.cpp`
   - compat warning/fallback 用例改为 alias-rejected / alias-present fail-fast。
2. `packages/runtime-host/tests/unit/runtime/recipes/TemplateFileRepositoryTest.cpp`
   - 改为 canonical-only 仓储行为与 legacy file hard-fail。
3. `packages/test-kit/src/test_kit/workspace_validation.py`
   - `control-runtime-compat-alias-rejected`
   - `control-tcp-server-compat-alias-rejected`
   - `control-cli-bootstrap-check-compat-alias-rejected`
   - 三个 app 入口均验证显式旧路径失败且不再出现 compat warning。
4. `tools/migration/validate_runtime_asset_cutover.py`
   - 终态改为 `config/machine_config.ini` / `templates.json` zero-hit gate。
5. `tools/migration/validate_wave2b_residual_payoff.py`
   - 删除 alias allowlist，4 个 blocking family 全部切到 zero-hit gate。

### 4C residual ledger / docs 清零

1. `tools/migration/wave2b_residual_ledger.json`
   - 收敛到终态：`entries=[]`。
2. `tools/migration/validate_wave2b_residuals.py`
   - 终态要求 `entry_count=0`、`present_entry_count=0`、`issue_count=0`。
3. 当前批次清理的 residual 文档：
   - `packages/process-runtime-core/BOUNDARIES.md`
   - `packages/simulation-engine/BOUNDARIES.md`
   - `packages/traceability-observability/README.md`
   - `packages/shared-kernel/README.md`
   - `packages/device-adapters/README.md`
   - `apps/hmi-app/DEPENDENCY_AUDIT.md`

## 3. 实际执行命令

### 静态校验

1. `python -m py_compile packages/test-kit/src/test_kit/workspace_validation.py tools/migration/validate_workspace_layout.py tools/migration/validate_runtime_asset_cutover.py tools/migration/validate_wave2b_residual_payoff.py tools/migration/validate_wave2b_residuals.py`
2. `python tools/migration/validate_workspace_layout.py`
3. `python tools/migration/validate_runtime_asset_cutover.py`
4. `python tools/migration/validate_wave2b_residual_payoff.py`
5. `python tools/migration/validate_wave2b_residuals.py`

### 构建与单测

1. `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile Local -Suite packages`
2. `C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build\bin\Debug\siligen_runtime_host_unit_tests.exe`
3. `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile CI -Suite apps`

### 根级 suites

1. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite packages -ReportDir integration\reports\verify\wave2b-batch4\packages -FailOnKnownFailure`
2. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite apps -ReportDir integration\reports\verify\wave2b-batch4\apps -FailOnKnownFailure`
3. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite integration -ReportDir integration\reports\verify\wave2b-batch4\integration -FailOnKnownFailure`
4. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite protocol-compatibility -ReportDir integration\reports\verify\wave2b-batch4\protocol -FailOnKnownFailure`
5. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite simulation -ReportDir integration\reports\verify\wave2b-batch4\simulation -FailOnKnownFailure`

## 4. 关键证据

1. `tools/migration/validate_workspace_layout.py`
   - `entry_count=13`
   - `failed_provenance_count=0`
   - `failed_wiring_count=0`
2. `tools/migration/validate_runtime_asset_cutover.py`
   - `config/machine_config.ini -> none`
   - `templates.json -> none`
   - `issue_count=0`
3. `tools/migration/validate_wave2b_residual_payoff.py`
   - 4 个 blocking family 均为 `none`
   - `issue_count=0`
4. `tools/migration/validate_wave2b_residuals.py`
   - `entry_count=0`
   - `present_entry_count=0`
   - `issue_count=0`
5. `siligen_runtime_host_unit_tests.exe`
   - `28 tests from 4 test suites`
   - `PASSED 28 tests`
6. `packages` suite
   - `passed=10 failed=0 known_failure=0 skipped=0`
   - `workspace-layout-boundary`
   - `wave2b-residual-ledger`
   - `wave2b-residual-payoff`
   - `runtime-asset-cutover`
   - 以上终态 gate 全部通过
7. `apps` suite
   - `passed=23 failed=0 known_failure=0 skipped=0`
   - `control-runtime-compat-alias-rejected`
   - `control-tcp-server-compat-alias-rejected`
   - `control-cli-bootstrap-check-compat-alias-rejected`
   - `control-runtime-subdir-canonical-config`
   - `control-tcp-server-subdir-canonical-config`
   - `control-cli-bootstrap-check-subdir`
   - 表明 canonical 相对路径合同仍成立，显式 compat alias 已被拒绝
8. 其余 suites
   - `integration passed=1 failed=0 known_failure=0 skipped=0`
   - `protocol-compatibility passed=1 failed=0 known_failure=0 skipped=0`
   - `simulation passed=5 failed=0 known_failure=0 skipped=0`

## 5. 结果摘要

1. root source-root selector 已被删除，workspace root 成为唯一有效 superbuild source root。
2. `config/machine_config.ini` alias 已删除，仓内 root alias 文件与 build/install staging 已一起移除。
3. `templates.json` fallback 已删除，现场若残留 legacy 单文件将直接 fail-fast。
4. `siligen_device_hal_persistence` / `siligen_persistence` alias 已删除，不再允许 compatibility 壳存在。
5. residual ledger 已进入终态空账本，相关 doc residual 也已从当前 owner 文档中清零。
6. Batch 4 的退出条件已满足，Wave 2B 全部完成。

## 6. 非阻断记录

1. 构建期间仍可见 protobuf/abseil 与 gtest 的既有链接警告；本批次未改变其结论。
2. `logs/*` 路径统一策略仍不在本批次范围内。
