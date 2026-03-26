# Phase 8: runtime-execution bridge-drain

更新时间：`2026-03-25`

## 1. 目标

在不触碰 `workflow/process-runtime-core` 重耦合内核的前提下，继续降低 `runtime-execution` 历史桥接目录的构建地位。

本轮目标不是删除 bridge 目录，而是把真实 target 定义迁到 canonical 目录，让 bridge 目录只保留 forwarding 责任。

## 2. 已完成项

### 2.1 canonical build roots 落位

以下 canonical 目录已补齐真实 `CMakeLists.txt`：

- `modules/runtime-execution/contracts/device/CMakeLists.txt`
- `modules/runtime-execution/adapters/device/CMakeLists.txt`
- `modules/runtime-execution/runtime/host/CMakeLists.txt`

这些文件现在承载 `siligen_device_contracts`、`siligen_device_adapters`、`siligen_runtime_host` 的真实 target 定义。

### 2.2 模块顶层切换为 canonical-first

`modules/runtime-execution/CMakeLists.txt` 已改为优先装配：

- `contracts/device`
- `adapters/device`
- `runtime/host`

因此，模块 owner 级装配不再默认从 `device-contracts`、`device-adapters`、`runtime-host` 三个 bridge 子树进入。

### 2.3 bridge CMake 降级为 forwarder

以下历史 bridge 目录的 `CMakeLists.txt` 已降级为 forwarding 层：

- `modules/runtime-execution/device-contracts/CMakeLists.txt`
- `modules/runtime-execution/device-adapters/CMakeLists.txt`
- `modules/runtime-execution/runtime-host/CMakeLists.txt`

它们当前只负责把兼容入口转发到 canonical build roots。

### 2.4 runtime-host 测试承载面切到 canonical

`runtime-host/tests` 的真实测试文件已迁到：

- `modules/runtime-execution/runtime/host/tests`

当前口径调整为：

- `tests/CMakeLists.txt` 已直接把 `runtime/host/tests` 作为 canonical 测试根
- `modules/runtime-execution/runtime-host/tests/CMakeLists.txt` 已降级为 forwarding 层
- `modules/runtime-execution/tests/runtime-host/CMakeLists.txt` 仅保留模块级索引/转发责任

### 2.5 MultiCard vendor 资产切到 canonical

MultiCard vendor 资产已迁到：

- `modules/runtime-execution/adapters/device/vendor/multicard`

当前口径调整为：

- 根 `CMakeLists.txt`、`adapters/device`、`runtime/host` 已改为 canonical-first vendor 路径
- `modules/runtime-execution/device-adapters/vendor/multicard/README.md` 仅保留 bridge 标记，不再承载真实资产 owner 事实
- 若外部缓存仍显式覆写 `SILIGEN_MULTICARD_VENDOR_DIR`，仍可继续兼容

### 2.6 runtime-execution / workflow public-surface drain

`workflow` 已补齐 canonical public include 承载面：

- `modules/workflow/application/include`
- `modules/workflow/domain/include`
- `modules/workflow/adapters/include`

这些目录当前以 wrapper header 形式承接 `process-runtime-core`，并通过以下 public targets 对外暴露：

- `siligen_workflow_domain_public`
- `siligen_workflow_application_public`
- `siligen_workflow_adapters_public`
- `siligen_workflow_runtime_consumer_public`

本轮已完成以下显式依赖切换：

- `modules/runtime-execution/adapters/device`
- `modules/runtime-execution/runtime/host`
- `modules/runtime-execution/runtime/host/runtime/events`
- `modules/runtime-execution/runtime/host/runtime/scheduling`
- `apps/planner-cli`
- `apps/runtime-gateway/transport-gateway`
- 根 `CMakeLists.txt` 中的 `security_module`

上述消费者已退出对 `workflow/process-runtime-core/src` 的直接显式 include 依赖，统一转向 `workflow` canonical public surface。

## 3. 当前仍未切断的耦合

本轮 cutover 后，仍有以下残余 bridge 依赖：

1. `modules/workflow/process-runtime-core`
   - `workflow` 的 canonical public surface 当前仍由 wrapper header 回指到 `process-runtime-core` bridge-backed 实现。
   - 旧 `siligen_process_runtime_core*` target 仍作为内部实现聚合面存在，尚未完成 hard closeout。

因此，本轮正确结论是：

- 已完成 `runtime-execution` 的 canonical-first build cutover
- 已完成 `runtime-host/tests` 与 `device-adapters/vendor` 的 canonical owner 收敛
- 已完成 `runtime-execution` 对 `workflow/process-runtime-core/src` 的 direct include drain
- 未完成 `runtime-execution` 的 hard bridge exit

## 4. 校验口径

`scripts/migration/validate_workspace_layout.py` 已补充以下断言：

- `runtime-execution` 必须存在 canonical CMake roots：
  - `contracts/device/CMakeLists.txt`
  - `adapters/device/CMakeLists.txt`
  - `runtime/host/CMakeLists.txt`
  - `runtime/host/tests/CMakeLists.txt`
- `workflow` 必须存在 canonical public include roots：
  - `application/include/application`
  - `domain/include/domain`
  - `adapters/include/infrastructure`
  - `adapters/include/recipes`
- `modules/workflow/CMakeLists.txt` 必须提供 `siligen_workflow_*_public` targets，且使用 `LINK_ONLY` 方式隔离旧 target 的 `src` include 透传。
- `runtime-execution`、`planner-cli`、`transport-gateway` 与根 `security_module` 对应的 CMake 文件不得再显式命中 `process-runtime-core/src`。
- `device-contracts`、`device-adapters`、`runtime-host` 的 bridge `CMakeLists.txt` 必须带有 `legacy-source bridge forwarder` 标记。
- `runtime-host/tests/CMakeLists.txt` 也必须带有 bridge forwarder 标记。
- `adapters/device/vendor/multicard/README.md` 必须存在；bridge vendor README 必须保留 `legacy-source bridge` 标记。

## 5. 后续建议

若继续推进 `runtime-execution` 的 hard closeout，建议按以下顺序继续：

1. 将 `workflow` wrapper-backed public surface 继续物理收敛到真正的 canonical 实现目录，而不是回指 `process-runtime-core`
2. 清理 `modules/runtime-execution/device-adapters/vendor/multicard` 与 `modules/runtime-execution/runtime-host/tests` 剩余 bridge 空壳
3. 启动 `workflow` 的下一轮 bridge drain，继续降低 `process-runtime-core` 作为默认实现面的地位
