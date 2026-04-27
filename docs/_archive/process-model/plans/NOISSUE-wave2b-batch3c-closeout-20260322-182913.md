# NOISSUE Wave 2B Batch 3C Closeout

- 状态：Done
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-182913`

## 1. 总体裁决

1. `Wave 2B Batch 3C = Done`
2. `3C residual payoff / payoff gate = Go`
3. `Batch 4 进入裁决 = Go`
4. 本批次未删除 `config/machine_config.ini` compatibility alias，也未删除 `templates.json` fallback；删桥动作仍留给 `Batch 4`。

## 2. 本批次完成内容

### 3C-1 recipes/json consumer payoff

1. `packages/runtime-host/CMakeLists.txt`
   - 新增 canonical target：`siligen_runtime_recipe_persistence`。
   - `siligen_device_hal_persistence` 与 `siligen_persistence` 保留为 inbound compatibility alias。
   - `siligen_control_runtime_factories`、`siligen_control_runtime_infrastructure`、`packages/runtime-host/tests/CMakeLists.txt` 全部切到 canonical target。
2. `packages/transport-gateway/CMakeLists.txt`
   - 删除 raw `${SILIGEN_THIRD_PARTY_DIR}` 暴露。
   - 新增显式 `siligen_transport_gateway_json_headers` / `siligen_transport_gateway_boost_headers`，由 target 语义承接 json/boost 头依赖。
3. `apps/control-tcp-server/CMakeLists.txt`
   - 去掉 app 入口层对 `${SILIGEN_BOOST_INCLUDE_DIR}` 的重复直连，改由 `siligen_transport_gateway` 暴露所需接口。

### 3C-2 logging / third-party consumer payoff

1. `packages/traceability-observability/CMakeLists.txt`
   - 新增 `siligen_traceability_spdlog_headers`。
   - `siligen_spdlog_adapter` 改为通过 `${SILIGEN_SPDLOG_INCLUDE_DIR}` 暴露显式 spdlog 头依赖，不再直接写 `${SILIGEN_THIRD_PARTY_DIR}/spdlog/include`。
2. `packages/runtime-host/CMakeLists.txt`
   - 移除 `RUNTIME_HOST_COMMON_INCLUDE_DIRS` 中的 raw `${SILIGEN_THIRD_PARTY_DIR}`。
   - 新增 `siligen_runtime_host_boost_headers`，把 boost 依赖显式挂到 `siligen_control_runtime_config`、`siligen_runtime_host`、runtime events、runtime scheduling。
3. `packages/process-runtime-core/CMakeLists.txt`
   - 去掉 `PROCESS_RUNTIME_CORE_THIRD_PARTY_DIR` public 暴露。
   - 新增显式第三方变量：`SILIGEN_NLOHMANN_INCLUDE_DIR`、`SILIGEN_RUCKIG_SOURCE_DIR`、`SILIGEN_PROTOBUF_SOURCE_DIR`、`SILIGEN_GTEST_SOURCE_DIR`。
   - 新增 `siligen_process_runtime_core_boost_headers` / `siligen_process_runtime_core_json_headers`，并把 boost/json 依赖挂到真实 consumer target。
4. `CMakeLists.txt`
   - 根级 superbuild 从 raw `${SILIGEN_THIRD_PARTY_DIR}` 透传切到显式变量：`SILIGEN_BOOST_INCLUDE_DIR`、`SILIGEN_NLOHMANN_INCLUDE_DIR`、`SILIGEN_SPDLOG_INCLUDE_DIR`。

### 3C-3 payoff gate 收紧

1. 新增 `tools/migration/validate_wave2b_residual_payoff.py`
   - 专门校验 4 个 `must_zero_before_cutover=true` family 的 live runtime/build debt。
   - 仅扫描 `packages/`、`apps/`、`tools/` 下 code/CMake/script 文件，不扫描 docs。
   - 允许 `siligen_device_hal_persistence` / `siligen_persistence` 只在 `packages/runtime-host/CMakeLists.txt` 作为 alias 定义存在。
2. `packages/test-kit/src/test_kit/workspace_validation.py`
   - `packages` suite 新增 `wave2b-residual-payoff` case。
3. `tools/migration/validate_wave2b_residuals.py`
   - 排除新 payoff validator 自身，避免 ledger gate 与 payoff gate 互相污染。

## 3. 实际执行命令

### 静态校验

1. `python -m py_compile tools/migration/validate_wave2b_residuals.py tools/migration/validate_wave2b_residual_payoff.py packages/test-kit/src/test_kit/workspace_validation.py`
2. `python tools/migration/validate_wave2b_residuals.py`
3. `python tools/migration/validate_wave2b_residual_payoff.py`
4. `python tools/migration/validate_runtime_asset_cutover.py`
5. `python tools/migration/validate_workspace_layout.py`

### 构建与单测

1. `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile Local -Suite packages`
2. `C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build\bin\Debug\siligen_runtime_host_unit_tests.exe`
3. `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile CI -Suite apps`

### 根级 suites

1. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite packages -ReportDir integration\reports\verify\wave2b-batch3c\packages -FailOnKnownFailure`
2. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite apps -ReportDir integration\reports\verify\wave2b-batch3c\apps -FailOnKnownFailure`
3. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite integration -ReportDir integration\reports\verify\wave2b-batch3c\integration -FailOnKnownFailure`
4. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite protocol-compatibility -ReportDir integration\reports\verify\wave2b-batch3c\protocol -FailOnKnownFailure`
5. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite simulation -ReportDir integration\reports\verify\wave2b-batch3c\simulation -FailOnKnownFailure`

## 4. 关键证据

1. `tools/migration/validate_wave2b_residual_payoff.py`
   - `device-hal-logging-owner = none`
   - `device-hal-recipes-owner = packages/runtime-host/CMakeLists.txt`，仅剩 alias 定义点
   - `process-runtime-core-third-party-consumer = none`
   - `recipe-json-third-party-consumer = none`
   - `issue_count=0`
2. `tools/migration/validate_wave2b_residuals.py`
   - `runtime_build_debt_must_zero_count=4`
   - `issue_count=0`
3. `packages` suite：
   - `passed=10 failed=0 known_failure=0 skipped=0`
   - 新增 `wave2b-residual-payoff` case 通过
4. `apps` suite：
   - `passed=23 failed=0 known_failure=0 skipped=0`
   - `control-tcp-server-subdir-canonical-config`
   - `control-tcp-server-compat-warning-once`
   - `control-cli-bootstrap-check-compat-warning-once`
   - 上述与 3B 合同相关用例均保持通过，说明 3C 未回退已有 bridge/flip 行为
5. `runtime-host` unit：
   - `30 tests from 4 test suites`
   - `PASSED 30 tests`
6. 其余根级 suites：
   - `integration passed=1 failed=0 known_failure=0 skipped=0`
   - `protocol-compatibility passed=1 failed=0 known_failure=0 skipped=0`
   - `simulation passed=5 failed=0 known_failure=0 skipped=0`

## 5. 结果摘要

1. `must_zero_before_cutover=true` 的 4 个 residual family 已全部清零 live debt。
2. recipes persistence canonical target 已从 legacy `device_hal` 命名收口到 `siligen_runtime_recipe_persistence`，同时保留 inbound alias 兼容。
3. logging / recipe / process-runtime-core / transport-gateway 的 raw `${SILIGEN_THIRD_PARTY_DIR}` 暴露已改为显式 target/variable 语义。
4. Batch 3 的退出条件已满足，后续可以进入 `Batch 4` 做删桥与封板。

## 6. 非阻断记录

1. `packages/runtime-host/CMakeLists.txt` 仍保留 `siligen_device_hal_persistence` / `siligen_persistence` alias 定义；这是兼容壳，不再有 live consumer，删除动作留给 `Batch 4`。
2. 构建期间仍可见 protobuf/abseil 与 gtest 的既有链接告警，以及 `spdlog` 触发的 `_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING` 警告；本批次未改变其结论。
3. `logs/*` 路径统一仍不在本批次范围内。

## 7. Batch 4 裁决

1. `3C` 的退出条件已满足：
   - 4 个 blocking residual family 的 live debt gate 已归零
   - recipes/json/logging/process-runtime-core third-party owner 语义已收口
   - 根级 suites 全绿
2. `Batch 4 = Go`
3. 下一步才允许推进：
   - bridge / fallback 删除
   - compatibility alias 收紧
   - 文档 residual 归并与最终 allowlist 删除
