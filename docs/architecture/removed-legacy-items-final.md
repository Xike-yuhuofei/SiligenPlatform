# Removed Legacy Items Final

更新时间：`2026-03-19`

本文档按目录记录本轮 legacy 删除最终状态。每个目录都包含：

- 删除前确认结果
- 实际删除内容
- 保留内容及原因
- 删除后回归验证结果

统一回滚备份：

- `D:\Projects\SiligenSuite\tmp\legacy-removal-backups\20260318-225801`
- `D:\Projects\SiligenSuite\tmp\legacy-removal-backups\20260319-074024`
- `D:\Projects\SiligenSuite\tmp\legacy-removal-backups\20260319-160038`

## 1. `dxf-editor`

### 删除前确认结果

- `dxf-editor/`、`apps/dxf-editor-app/`、`packages/editor-contracts/` 在本轮执行前已经不存在。
- `legacy-exit-check` 的 `removed-directories-absent` 规则在基线和最终验收中都为 `PASS`。
- 当前 DXF 编辑默认流程已经切到 `docs/runtime/external-dxf-editing.md` 记录的外部编辑器人工流程。

### 实际删除内容

- 本轮未新增删除；维持已删除状态。

### 保留内容及原因

- 工作区内未保留 `dxf-editor` 目录。
- 保留的是历史说明文档与外部编辑器流程文档，因为产品流程已经不再依赖内建编辑器目录。

### 删除后回归验证结果

- `integration/reports/ci/legacy-exit/legacy-exit-checks.md`：`removed-directories-absent=PASS`
- `integration/reports/ci/workspace-validation.md`：整仓 `passed=14 failed=0 known_failure=0`

## 2. `dxf-pipeline`

### 删除前确认结果

- `apps/hmi-app` 当前预览链路已经改为调用 `engineering_data.cli.generate_preview`。
- 顶层 `hmi-client` 预览链路已改为调用 `engineering_data.cli.generate_preview`。
- `packages/engineering-data` 已把 `dxf_pipeline.*` import shim 与 `proto.dxf_primitives_pb2` 兼容层迁入 `src/`。
- `packages/engineering-data/pyproject.toml` 已由 canonical 包直接暴露 legacy console script 名称。
- `packages/engineering-contracts/tests/test_engineering_contracts.py` 当前只验证 canonical proto / fixture / producer / consumer。
- `legacy-exit-check` 最终通过前，已清理运行/回滚文档中的活动依赖表述；最终 `failed_rules=0`。

### 实际删除内容

- `dxf-pipeline/` 整目录
- 当前工作区删除方式为移动到回滚备份：
  - `D:\Projects\SiligenSuite\tmp\legacy-removal-backups\20260319-074024\dxf-pipeline`

### 保留内容及原因

- 工作区内不再保留 `dxf-pipeline/` 目录。
- 继续保留 `packages/engineering-data/src/dxf_pipeline/**`
  - 原因：兼容历史 `dxf_pipeline.*` import，但 owner 已切到 canonical 包。
- 继续保留 `packages/engineering-data/src/proto/dxf_primitives_pb2.py`
  - 原因：兼容历史 `proto.dxf_primitives_pb2` import。
- 继续保留 `packages/engineering-data/pyproject.toml` 中 legacy CLI alias
  - 原因：兼容历史命令名，但安装 owner 已切到 canonical 包。

### 删除后回归验证结果

- 根级 `build`
  - 命令：`build.ps1 -Profile Local -Suite all`
  - 结果：`workspace build complete`
- 根级 `test`
  - 报告：`integration/reports/verify/dxf-pipeline-physical-delete-20260319-074739/root-test-final/workspace-validation.md`
  - 结果：`passed=18 failed=0 known_failure=0`
- `python integration\scenarios\run_engineering_regression.py`
  - `engineering regression passed`
- `python integration\protocol-compatibility\run_protocol_compatibility.py`
  - `protocol compatibility suite passed`
- `legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\verify\dxf-pipeline-physical-delete-20260319-074739\legacy-exit-final`
  - 报告：`integration/reports/verify/dxf-pipeline-physical-delete-20260319-074739/legacy-exit-final/legacy-exit-checks.md`
  - 结果：`passed_rules=9 failed_rules=0`
- 结论：整目录删除后未引入回归。

## 3. `simulation-engine`（旧顶层目录）

### 删除前确认结果

- 顶层 `simulation-engine/` 在本轮前已经不存在。
- 根级 `build.ps1`、`test.ps1`、`ci.ps1` 的仿真链路都指向 `packages/simulation-engine/`。
- 最终 CI 验收中的 `simulation-engine-smoke`、`simulation-engine-json-io`、`simulated-line` 全部通过。

### 实际删除内容

- 本轮未新增删除；维持顶层旧目录已删除状态。

### 保留内容及原因

- 保留 `packages/simulation-engine/`
  - 原因：它是当前仿真能力的 canonical package，不属于 legacy 旧顶层目录。

### 删除后回归验证结果

- `integration/reports/ci/workspace-validation.md`
  - `simulation-engine-smoke=passed`
  - `simulation-engine-json-io=passed`
  - `simulated-line=passed`
- 结论：旧顶层目录维持已删除状态，不影响当前仿真验收。

## 4. `control-core`

### 删除前确认结果

- `apps/control-runtime/run.ps1 -DryRun`、`apps/control-tcp-server/run.ps1 -DryRun`、`apps/control-cli/run.ps1 -DryRun` 都已输出 `source: canonical`。
- `apps/hmi-app/run.ps1 -DryRun -DisableGatewayAutostart` 已稳定输出 canonical HMI 入口，不再暴露 `control-core` 自动拉起来源。
- `integration/hardware-in-loop/run_hardware_smoke.py` 已默认使用 canonical `siligen_tcp_server.exe`。
- `control-core/apps/CMakeLists.txt` 当前只注册根级 `apps/*`，不再把 `control-core/apps/control-runtime`、`control-core/apps/control-tcp-server` 纳入默认构建。
- `packages/transport-gateway/tests/test_transport_gateway_compatibility.py` 已切到根级 `apps/control-tcp-server` 校验薄入口。
- `apps/hmi-app` 的默认 DXF 候选目录已收敛到 `uploads/dxf`、`packages/engineering-contracts/fixtures/cases/rect_diag` 与显式 `SILIGEN_DXF_DEFAULT_DIR`。
- 仍未满足删除条件的内容已经明确隔离在 `control-core/src/shared`、`src/infrastructure`、`modules/device-hal` 与 `third_party`。

### 实际删除内容

- `control-core/apps/control-runtime/`
- `control-core/apps/control-tcp-server/`
- `control-core/modules/shared-kernel/`
- `control-core/src/domain/`
- `control-core/src/application/`
- `control-core/modules/process-core/`
- `control-core/modules/motion-core/`

### 保留内容及原因

- `control-core/apps/CMakeLists.txt`
  - 原因：`control-core` 仍是当前 control app 的 CMake source root，需要继续注册根级 `apps/*`。
- `control-core/build/bin/**`
  - 原因：当前已无 provenance consumer；只是因为 `control-core/` 整目录尚未过删除门禁而随目录一起保留，不再构成独立保留理由。
- `control-core/src/shared/`、`control-core/src/infrastructure/`
  - 原因：当前仍分别承接 shared compat include root 与 infrastructure / DXF parsing owner；`packages/process-runtime-core`、`packages/runtime-host`、`packages/transport-gateway` 还未完全脱离。
- `control-core/modules/device-hal/`
  - 原因：`SpdlogLoggingAdapter.cpp` 与 recipes persistence / serializer 仍由 `control-core/src/infrastructure/CMakeLists.txt` 直接编译。
- `control-core/third_party/`
  - 原因：`control-core` 顶层 CMake 与 `packages/process-runtime-core` standalone 仍显式把它当 `Boost` / `Ruckig` / `Protobuf` / `spdlog` / `nlohmann/json` provider。
- `control-core/config/`、`control-core/data/recipes/`
  - 原因：活动 consumer 已归零；当前只是因为 `control-core/` 整目录尚未过门禁而随目录一起保留，不再构成独立保留理由。

### 删除后回归验证结果

- `integration/reports/verify/control-core-core-subtree-delete-20260319-160038/root-test/workspace-validation.md`
  - `passed=25 failed=1 known_failure=0`
- `integration/reports/verify/control-core-core-subtree-delete-20260319-160038/legacy-exit/legacy-exit-checks.md`
  - `passed_rules=17 failed_rules=0`
- `integration/reports/ci/workspace-validation.md`
  - `passed=21 failed=1 known_failure=0`
- `integration/reports/ci/legacy-exit/legacy-exit-checks.md`
  - `passed_rules=17 failed_rules=0`
- 独立命令：
  - `python integration\protocol-compatibility\run_protocol_compatibility.py`：`protocol compatibility suite passed`
  - `python integration\scenarios\run_engineering_regression.py`：`engineering regression passed`
  - `python integration\simulated-line\run_simulated_line.py`：`simulated-line regression passed: compat, scheme_c`
  - `python integration\hardware-in-loop\run_hardware_smoke.py`：`hardware smoke passed: TCP endpoint is reachable`
- 结论：删除 5 个 control-core legacy 子树后，未引入新增回归；唯一保留失败仍是既有 `device-shared-boundary`。

## 5. 最终结论

### 已完全删除

- `dxf-editor/`
- `dxf-pipeline/`
- 顶层 `simulation-engine/`

### 只删除了部分内容

- `control-core/`
  - 已删除 `apps/control-runtime/`、`apps/control-tcp-server/`
  - 已删除 `modules/shared-kernel/`、`src/domain/`、`src/application/`、`modules/process-core/`、`modules/motion-core/`
  - 其余核心目录保留。

### 仍不能删以及原因

- `control-core/` 剩余核心目录
  - 原因：`control-core` 仍是 live CMake / source-root / third-party owner；`control-core/src/infrastructure/CMakeLists.txt` 仍直接编译 `modules/device-hal` 下的 logging / recipes 实现，`control-core/src/shared` 仍是 compat include root。
