# Legacy Removal Execution

更新时间：`2026-03-19`

> 本文档是 `2026-03-19` 的历史执行记录，保留当时 legacy 目录物理删除、回滚准备和批次验证的过程证据。
>
> 当前正式状态不要以本文为准。需要查看当前真值时，优先使用：
>
> - [docs/architecture/legacy-cutover-status.md](../../../architecture/legacy-cutover-status.md)
> - [docs/architecture/governance/migration/legacy-deletion-gates.md](../../../architecture/governance/migration/legacy-deletion-gates.md)
> - [docs/architecture/governance/migration/legacy-exit-checks.md](../../../architecture/governance/migration/legacy-exit-checks.md)

## 1. 执行范围

本次按以下顺序执行 legacy 删除与验收：

1. `dxf-editor`
2. `dxf-pipeline`
3. `simulation-engine`（旧顶层目录）
4. `control-core`

执行依据：

- `docs/architecture/governance/migration/legacy-deletion-gates.md`
- `docs/architecture/governance/migration/legacy-exit-checks.md`
- `docs/architecture/history/progress/dxf-editor-strangler-progress.md`
- `docs/architecture/history/progress/dxf-pipeline-strangler-progress.md`
- `docs/architecture/history/closeouts/process-runtime-core-cutover.md`

补充说明：

- 用户给出的 `docs/architecture/simulation-engine-strangler-progress.md` 在当前工作区不存在；本次对 `simulation-engine` 的判定改用 `legacy-deletion-gates.md`、根级 build/test/CI 入口与工作区实体状态共同确认。

## 2. 回滚准备

- 统一回滚备份根：
  - `D:\Projects\SiligenSuite\tmp\legacy-removal-backups\20260318-225801`
  - `D:\Projects\SiligenSuite\tmp\legacy-removal-backups\20260319-074024`
  - `D:\Projects\SiligenSuite\tmp\legacy-removal-backups\20260319-160038`
- `dxf-pipeline` 外围目录与 `control-core` app 壳删除使用第一批备份根。
- `dxf-pipeline` 整目录物理删除使用第二批备份根。
- `control-core` 核心子树删除使用第三批备份根。
- 执行中未出现回归，因此未触发回滚；若后续需要恢复，可直接把对应目录移回原路径。

## 3. 批次执行记录

### 3.1 `dxf-editor`

- 处理方式：只做删除前确认与删除后验收，不做新删除。
- 原因：`dxf-editor/`、`apps/dxf-editor-app/`、`packages/editor-contracts/` 已在本轮前不存在。

### 3.2 `dxf-pipeline`

- 先删外围，再完成 import/CLI cutover，最后再做整目录物理删除。
- 实际删除：
  - 第一阶段：`dxf-pipeline/.github/`、`benchmarks/`、`docs/`、`examples/`、`proto/`、`scripts/`、`tests/`、`tmp/`
  - 第二阶段：`dxf-pipeline/` 整目录
  - 当前回滚路径：`D:\Projects\SiligenSuite\tmp\legacy-removal-backups\20260319-074024\dxf-pipeline`
- 删除前确认：
  - `apps/hmi-app`、顶层 `hmi-client` 已切到 `engineering_data.cli.generate_preview`
  - `dxf_pipeline.*` import shim 与 `proto.dxf_primitives_pb2` 已迁入 `packages/engineering-data/src`
  - legacy CLI 名称已由 `packages/engineering-data/pyproject.toml` 承接
  - `legacy-exit-check` 最终已确认：无 build/import/script/doc 回流
- 删除后验证：
  - `build.ps1 -Profile Local -Suite all`
  - `test.ps1 -Profile Local -Suite all -ReportDir integration\reports\verify\dxf-pipeline-physical-delete-20260319-074739\root-test-final`
  - `python integration\protocol-compatibility\run_protocol_compatibility.py`
  - `python integration\scenarios\run_engineering_regression.py`
  - `legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\verify\dxf-pipeline-physical-delete-20260319-074739\legacy-exit-final`
- 结果：
  - 根级 `build`：通过；`workspace build complete`
  - `workspace-validation`：`passed=18 failed=0 known_failure=0`
  - `protocol compatibility`：`protocol compatibility suite passed`
  - `integration`：`engineering regression passed`
  - `legacy-exit-check`：`passed_rules=9 failed_rules=0`

### 3.3 `simulation-engine`（旧顶层目录）

- 处理方式：只做删除前确认与删除后验收，不做新删除。
- 原因：顶层 `simulation-engine/` 已在本轮前不存在，当前 canonical 目录为 `packages/simulation-engine/`。

### 3.4 `control-core`

- 实际删除：
  - `control-core/apps/control-runtime/`
  - `control-core/apps/control-tcp-server/`
  - `control-core/modules/shared-kernel/`
  - `control-core/src/domain/`
  - `control-core/src/application/`
  - `control-core/modules/process-core/`
  - `control-core/modules/motion-core/`
- 整目录物理删除：
  - 未执行
  - 原因：最新审计仍确认 `control-core` 继续承担 `src/shared`、`src/infrastructure`、`modules/device-hal`、`third_party` 与顶层 CMake/source-root owner。
- 删除后验证：
  - `build.ps1 -Profile Local -Suite all`
  - `test.ps1 -Profile Local -Suite all -ReportDir integration\reports\verify\control-core-core-subtree-delete-20260319-160038\root-test`
  - `ci.ps1 -Suite all`
  - `python integration\protocol-compatibility\run_protocol_compatibility.py`
  - `python integration\scenarios\run_engineering_regression.py`
  - `python integration\simulated-line\run_simulated_line.py`
  - `python integration\hardware-in-loop\run_hardware_smoke.py`
  - `legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\verify\control-core-core-subtree-delete-20260319-160038\legacy-exit`
- 结果：
  - 根级 `build`：通过；`workspace build complete`
  - 删除后 `workspace-validation`：`passed=25 failed=1 known_failure=0`
  - 删除后 `legacy-exit-check`：`rules=17 failed_rules=0 findings=0`
  - `protocol compatibility`：`protocol compatibility suite passed`
  - `engineering regression`：`engineering regression passed`
  - `simulated-line`：`simulated-line regression passed: compat, scheme_c`
  - `hardware smoke`：`hardware smoke passed: TCP endpoint is reachable`
  - `ci.ps1 -Suite all`：`legacy-exit=17/17 PASS`，`workspace-validation=passed 21 failed 1`
  - 唯一保留失败仍是既有 `device-shared-boundary`，命中 `control-core/modules/device-hal` 与 device/shared seam；未出现本批删除引入的新失败

## 4. 门禁修正

执行前，`legacy-exit-check` 基线存在 1 条既有失败：

- `packages/process-runtime-core/CMakeLists.txt` 已是文档里确认过的 `control-core` CMake 阻塞项，但未加入自动化白名单。

本次已同步修正：

- `tools/scripts/legacy_exit_checks.py`
- `docs/architecture/governance/migration/legacy-exit-checks.md`
- 新增 `control-core/modules/shared-kernel`、`control-core/src/domain`、`control-core/src/application`、`control-core/modules/process-core`、`control-core/modules/motion-core` 的“目录必须不存在 + direct-path 不得回流”门禁

修正后，local 与 CI 的 `legacy-exit-check` 均为 `0` 失败；当前规则数为 `17`。

## 5. 最终验收

执行命令：

```powershell
.\ci.ps1 -Suite @('apps','packages','integration','protocol-compatibility','simulation')
```

结果：

- `integration/reports/ci/legacy-exit/legacy-exit-checks.md`：`passed_rules=17 failed_rules=0`
- `integration/reports/ci/workspace-validation.md`：`passed=21 failed=1 known_failure=0`
- 保留失败仍是既有 `device-shared-boundary`；本轮未引入新增回归，也未触发回滚

## 6. 当前结论

### 6.1 已完全删除

- `dxf-editor/`
- `dxf-pipeline/`
- 顶层 `simulation-engine/`

### 6.2 已部分删除

- `control-core/`
  - 已删除 `apps/control-runtime/`、`apps/control-tcp-server/`
  - 已删除 `modules/shared-kernel/`、`src/domain/`、`src/application/`、`modules/process-core/`、`modules/motion-core/`

### 6.3 仍不能删

- `control-core/` 顶层及核心实现目录
  - 原因：`control-core` 仍是 live CMake / source-root / third-party owner；`control-core/src/infrastructure/CMakeLists.txt` 仍直接编译 `modules/device-hal` 下的 logging / recipes 实现，`src/shared` 仍是 compat include root
