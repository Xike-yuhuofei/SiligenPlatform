# NOISSUE Wave 2B Batch 3B Runtime Asset Cutover Plan

- 状态：Implemented
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-180220`
- 前置批次：`docs/process-model/plans/NOISSUE-wave2b-batch3a-closeout-20260322-173504.md`

## 1. 批次目标

1. 将 runtime config bridge 解析 owner 收口到 `BuildContainer(...)` 边界。
2. 保持 `config/machine_config.ini` 为 warning-only inbound compatibility alias，不在本批次 hard-fail。
3. 将 `templates.json` 保持为只读 fallback，但补齐 canonical/legacy 冲突 fail-fast。
4. 为 `3B` 增加静态 gate 与 apps/packages 回归合同，确保进入 `3C` 前边界可持续验证。

## 2. 实施拆分

### 3B-1 Config bridge owner 收口

1. `packages/runtime-host/src/ContainerBootstrap.cpp`
   - 统一执行 `ResolveConfigFilePathWithBridge(...)`
   - 统一发出 compat alias warning
   - 将 resolved canonical path 传给 container 和 infrastructure builder
2. `apps/control-runtime/main.cpp`
   - 删除入口层重复 resolve/warning
   - 从 `BuildContainer(...)` 获取 resolved config path 作为运行时输出
3. `apps/control-tcp-server/main.cpp`
   - 删除入口层重复 resolve/warning
   - 从 `BuildContainer(...)` 获取 resolved config path 作为 TCP host config
4. `packages/runtime-host/src/container/ApplicationContainer.cpp`
   - 移除构造期重复 resolve/warning
5. `packages/runtime-host/src/bootstrap/InfrastructureBindingsBuilder.cpp`
   - 假设输入已解析完成，不再二次 resolve/warning

### 3B-2 Template legacy fallback 收紧

1. `packages/runtime-host/src/runtime/recipes/TemplateFileRepository.cpp`
   - canonical `templates/*.json` 与 legacy `templates.json` 先合并到统一 inventory
   - same id 或 same name 且内容冲突时 fail-fast
   - 等价内容允许并存，canonical 优先，legacy 仅补漏
   - `save` 仍只写 canonical
2. `packages/runtime-host/tests/unit/runtime/recipes/TemplateFileRepositoryTest.cpp`
   - 补 conflict/equivalent 回归

### 3B-3 验证门禁补齐

1. `tools/migration/validate_runtime_asset_cutover.py`
   - 仅扫描代码/脚本
   - `config/machine_config.ini` 仅允许出现在 `WorkspaceAssetPaths.h`
   - `templates.json` 仅允许出现在 `TemplateFileRepository.cpp` 及其 test
2. `packages/test-kit/src/test_kit/workspace_validation.py`
   - 接入 `runtime-asset-cutover` packages case
   - 增加 runtime/tcp-server 在子目录下的 canonical 相对配置合同
   - 增加 runtime/tcp-server/cli compat alias warning-only 且“恰好一次”回归
3. `packages/runtime-host/tests/unit/runtime/configuration/WorkspaceAssetPathsTest.cpp`
   - 补 root marker 不完整 / 找不到 workspace root 负向用例

## 3. 退出条件

1. `BuildContainer(...)` 成为 runtime path bridge 的唯一 owner。
2. runtime/tcp-server/cli compat alias 行为保持 warning-only，且每进程仅 1 次告警。
3. `TemplateFileRepository` 对 canonical/legacy 冲突 fail-fast。
4. `validate_runtime_asset_cutover.py` 通过。
5. `apps/packages/integration/protocol-compatibility/simulation` 五个 root suite 全绿。
6. 满足后才允许进入 `3C`。
