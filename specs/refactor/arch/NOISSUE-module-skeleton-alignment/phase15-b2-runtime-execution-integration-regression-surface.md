# Phase 15: runtime-execution integration/regression surface

更新时间：`2026-03-25`

## 目标

将 `runtime-execution` 的 integration/regression 承载面从占位目录提升为正式测试入口，并补齐一个不依赖真实硬件的 host 级 smoke 场景。

## 本轮变更

### 1. 模块级 tests 根正式化

新增以下模块级测试入口：

- `modules/runtime-execution/tests/CMakeLists.txt`
- `modules/runtime-execution/tests/integration/CMakeLists.txt`
- `modules/runtime-execution/tests/regression/CMakeLists.txt`

仓库级 `tests/CMakeLists.txt` 已切换为登记 `modules/runtime-execution/tests`，不再直接把 `runtime/host/tests` 作为 `runtime-execution` 的唯一测试根。

### 2. host integration smoke 落地

新增 canonical integration 目标：

- `modules/runtime-execution/runtime/host/tests/integration/CMakeLists.txt`
- `modules/runtime-execution/runtime/host/tests/integration/HostBootstrapSmokeTest.cpp`

当前目标名称：

- `runtime_execution_integration_host_bootstrap_smoke`

该 smoke 场景使用临时工作区与 `config/machine/machine_config.ini` 的 `Mock` 配置，调用 `BuildContainer(...)` 验证：

- canonical 配置路径可从非根工作目录正确解析
- host bootstrap 可完成最小装配
- 不依赖真实板卡即可拿到关键运行时端口
- recipe schema 目录可在工作区内按需落位

### 3. regression 面正式建根

新增 canonical regression 入口：

- `modules/runtime-execution/runtime/host/tests/regression/CMakeLists.txt`

当前先提供正式 target 聚合点：

- `runtime_execution_regression_tests`

本轮不扩展真实硬件回归矩阵，避免越过 `A3/A4` 的适配器 owner 化边界。

## ctest 登记口径

预期 `ctest -N` 至少新增以下可见测试项：

- `runtime_execution_integration_host_bootstrap_smoke`

`runtime_execution_regression_tests` 为聚合 target，不直接作为 `ctest` 用例出现。

## 仍待后补的硬件前置验证项

以下场景依赖 `A3/A4` 完成后继续补齐：

- 基于真实 `adapters/device` canonical owner 的板卡连接/断连 smoke
- 真实 MultiCard / IO / interlock 读写链路回归
- host bootstrap 在真实硬件模式下的启动失败面与恢复面验证
- 真实设备参与的 recipe/upload/event 端到端最小闭环
