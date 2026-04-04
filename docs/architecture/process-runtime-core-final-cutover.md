# process-runtime-core Final Cutover

更新时间：`2026-03-19`

## 1. 目标与边界

本轮目标是把 `packages/process-runtime-core` 固化为核心业务实现的真实 source root，并切断默认构建与默认测试对以下 legacy 目录的真实源码依赖：

- `control-core/src/domain`
- `control-core/src/application`
- `control-core/modules/process-core`
- `control-core/modules/motion-core`

本轮明确保持以下边界不变：

- 不把 `runtime-host`、`transport-gateway`、device 实现重新卷回 core
- 保持 `process / planning / execution / machine / safety / recipes / dispensing / diagnostics-domain / application` 的包内边界
- 不接受仅复制代码或仅更新 README；必须切换真实构建与真实测试

## 2. 核心源码承接范围

`packages/process-runtime-core` 当前已经承担以下真实源码 owner：

| 承接面 | package 内真实落点 | 当前 canonical target |
|---|---|---|
| process | `modules/process-core` | `siligen_process_core` |
| safety | `modules/motion-core/src/safety` | `siligen_motion_core` |
| motion / planning / execution | `src/domain/motion`、`src/domain/planning`、`src/domain/trajectory` | `siligen_motion`、`domain_trajectory`、`siligen_motion_execution_services` |
| machine | `src/domain/machine` | `domain_machine` |
| recipes | `src/domain/recipes` + `modules/process-core/src/recipes` | `domain_recipes`、`siligen_recipe_domain_services`、`siligen_recipe_core` |
| dispensing | `src/domain/dispensing` | `domain_dispensing`、`siligen_dispensing_execution_services`、`siligen_valve_core` |
| diagnostics-domain | `src/domain/diagnostics` | `siligen_diagnostics_domain_services` |
| application | `src/application` | `siligen_control_application` |

当前 package 对外已经通过以下 interface target 暴露分层消费面，而不是让消费者继续指向 legacy 路径：

- `siligen_process_runtime_core_process`
- `siligen_process_runtime_core_planning`
- `siligen_process_runtime_core_execution`
- `siligen_process_runtime_core_machine`
- `siligen_process_runtime_core_safety`
- `siligen_process_runtime_core_recipes`
- `siligen_process_runtime_core_dispensing`
- `siligen_process_runtime_core_diagnostics_domain`
- `siligen_process_runtime_core_application`
- `siligen_process_runtime_core`

## 3. 本轮完成的核心切换面

### 3.1 `process-runtime-core` CMake source root 切换

已完成：

1. `packages/process-runtime-core/CMakeLists.txt` 支持 package 作为独立 source root 直接配置，不再要求从 `control-core/` 顶层进入。
2. package standalone 配置时显式注入：
   - `SILIGEN_WORKSPACE_ROOT`
   - `SILIGEN_CONTROL_CORE_DIR`
   - `SILIGEN_CONTROL_CORE_SRC_DIR`
   - `SILIGEN_CONTROL_CORE_PCH_HEADER`
   - `SILIGEN_SHARED_COMPAT_INCLUDE_ROOT`
   - `SILIGEN_THIRD_PARTY_DIR`
   - `SILIGEN_BOOST_INCLUDE_DIR`
   - `SILIGEN_PROCESS_RUNTIME_CORE_DIR`
3. package source root 只认：
   - `packages/process-runtime-core/src`
   - `packages/process-runtime-core/modules/process-core/include`
   - `packages/process-runtime-core/modules/motion-core/include`
4. package 不再允许回落到 `CMAKE_SOURCE_DIR/src` 或 `CMAKE_SOURCE_DIR/third_party`。
5. `control-core/CMakeLists.txt` 已把全局 `include_directories(${CMAKE_SOURCE_DIR}/src ...)` 从 package 之前移到 package 之后，切断 package 对 `control-core/src/domain`、`src/application` 的隐式全局 include 继承。
6. `control-core/modules/CMakeLists.txt` 已不再 fallback 到 legacy `modules/process-core` / `modules/motion-core`；现在要求 canonical target 必须先由 package 提供。

### 3.2 单测 / 集成测试入口切换

已完成：

1. `packages/process-runtime-core/tests/CMakeLists.txt` 是 `siligen_unit_tests` 与 `siligen_pr1_tests` 的真实定义点。
2. `packages/process-runtime-core/tests/CMakeLists.txt` 新增聚合目标 `process_runtime_core_tests`，统一承接 package 核心测试入口。
3. `control-core/tests/CMakeLists.txt` 只保留 canonical 转发，不再定义 process runtime 的真实测试目标。
4. `packages/process-runtime-core/CMakeLists.txt` 新增统一的 `process_runtime_core_add_test_with_runtime_environment(...)`，为 Windows 下的 `ctest` 注入运行时 DLL 搜索路径，避免测试继续依赖手工 `PATH`。
5. standalone 测试依赖仍只拉起：
   - `packages/shared-kernel`
   - `control-core/src/shared`
   - `control-core/src/infrastructure/adapters/planning/dxf`
   - `third_party/protobuf`
   - GoogleTest

说明：

- 当前仍未把 `control-core/src/shared` 和 `control-core/src/infrastructure/adapters/planning/dxf` 迁出；它们是 shared compat 与 DXF parsing adapter 的真实 owner，不属于本轮要删除的四个 legacy 目录，但仍是 `control-core` 尚不可整体删除的原因。

### 3.3 include / link 对 legacy 的清理

已完成：

1. `packages/process-runtime-core` 自身不再通过 `../../control-core/...` 指向 `src/domain`、`src/application`、`modules/process-core`、`modules/motion-core`。
2. `control-core/src/shared/CMakeLists.txt` 已改成优先使用：
   - `SILIGEN_SHARED_COMPAT_INCLUDE_ROOT`
   - `SILIGEN_THIRD_PARTY_DIR`
   - `SILIGEN_CONTROL_CORE_PCH_HEADER`
   不再硬编码 `CMAKE_SOURCE_DIR/src` / `CMAKE_SOURCE_DIR/third_party`。
3. `control-core/src/infrastructure/adapters/planning/dxf/CMakeLists.txt` 已改成显式依赖：
   - package `src`
   - shared compat include root
   - workspace proto 根
   不再把 `CMAKE_SOURCE_DIR/src` 当作 package source root。
4. 以下 consumer 已从 `SILIGEN_CONTROL_CORE_SRC_DIR` 切到 `SILIGEN_SHARED_COMPAT_INCLUDE_ROOT`，不再拿整个 `control-core/src` 当 include 根：
   - `apps/control-cli`
   - `apps/control-runtime`
   - `apps/control-tcp-server`
   - `packages/runtime-host`
   - `packages/runtime-host/src/runtime/events`
   - `packages/runtime-host/src/runtime/scheduling`
   - `packages/transport-gateway`

当前仍保留的 include 风险：

1. `control-core/src` 仍以 shared compat include 根的形式出现在 compile command 中。
2. standalone `build.ninja` 里仍能看到：
   - `-ID:\Projects\SiligenSuite\control-core\src` 命中 `148` 次
   - `-ID:\Projects\SiligenSuite\control-core\src\shared\types` 命中 `148` 次
   - `-ID:\Projects\SiligenSuite\control-core\src\shared\Geometry` 命中 `110` 次
3. 这说明本轮已经切断了四个 legacy 目录的真实编译 owner，但还没有让整个 `control-core/src` 退出 include 图。

## 4. 验证结果

### 4.1 package standalone configure / build / test

执行：

```powershell
cmake -S D:\Projects\SiligenSuite\packages\process-runtime-core `
  -B D:\Projects\SiligenSuite\tmp\process-runtime-core-standalone-clangcl-check `
  -G Ninja `
  -DBUILD_TESTING=ON `
  -DSILIGEN_USE_PCH=OFF `
  -DSILIGEN_ENABLE_CGAL=OFF

cmake --build D:\Projects\SiligenSuite\tmp\process-runtime-core-standalone-clangcl-check `
  --target process_runtime_core_tests `
  --config Debug -- -j 1

ctest --test-dir D:\Projects\SiligenSuite\tmp\process-runtime-core-standalone-clangcl-check `
  -C Debug `
  -R "(siligen_(unit|pr1)_tests|process_runtime_core_deterministic_path_execution_test)" `
  --output-on-failure
```

结果：

- standalone configure 成功
- standalone build 成功
- `ctest` 3/3 通过：
  - `process_runtime_core_deterministic_path_execution_test`
  - `siligen_unit_tests`
  - `siligen_pr1_tests`

### 4.2 standalone 构建图命中统计

对 `D:\Projects\SiligenSuite\tmp\process-runtime-core-standalone-clangcl-check\build.ninja` 检查结果：

| 模式 | 命中次数 |
|---|---:|
| `control-core\src\domain` | `0` |
| `control-core\src\application` | `0` |
| `control-core\modules\process-core` | `0` |
| `control-core\modules\motion-core` | `0` |
| `packages\process-runtime-core\src\domain` | `195` |
| `packages\process-runtime-core\src\application` | `98` |
| `packages\process-runtime-core\modules\process-core` | `141` |
| `packages\process-runtime-core\modules\motion-core` | `139` |

结论：

1. 当前 standalone 构建图里，四个 legacy 目录已经不再命中真实编译规则。
2. `packages/process-runtime-core` 已经是这些核心实现的真实构建 source root。

### 4.3 `control-core` 顶层验证

执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File D:\Projects\SiligenSuite\build.ps1 `
  -Profile Local `
  -Suite all
```

结果：

1. 删除前 fresh provenance 审计阶段，`control-core` 顶层 configure / generate 成功。
2. 删除后根级 `build.ps1 -Profile Local -Suite all` 重新通过，`modules` 阶段继续明确输出：
   - `[Modules] process-core / motion-core 由 packages/process-runtime-core 提供 canonical target`
3. 默认工作区 build 已不再依赖 `control-core/src/domain`、`src/application`、`modules/process-core`、`modules/motion-core` 的物理存在。

结论：

- `control-core` 顶层对 package canonical target 的接入是成立的。
- 删除前要求的 workspace 级全量重建证明已经补齐，不再阻止四个 legacy 目录物理删除。

## 5. 删除前最后的 fresh provenance 结论

在 `2026-03-19` 删除前，以下证据同时成立：

1. live 代码、脚本、CMake 对 `control-core/src/domain`、`control-core/src/application`、`control-core/modules/process-core`、`control-core/modules/motion-core` 的 direct-path 命中为 `0`。
2. `D:\Projects\SiligenSuite\tmp\control-core-provenance-20260319-160038\build.ninja` 与 `D:\Projects\SiligenSuite\tmp\process-runtime-core-standalone-20260319-160038\build.ninja` 对这四个目录的 source/object 命中都为 `0`。
3. `packages/process-runtime-core` standalone configure/build/ctest 通过。
4. 根级 `build.ps1 -Profile Local -Suite all` 在当前工作区可重建。

基于以上证据，这四个 legacy 目录已在删除前备份到：

- `D:\Projects\SiligenSuite\tmp\legacy-removal-backups\20260319-160038\control-core\src\domain`
- `D:\Projects\SiligenSuite\tmp\legacy-removal-backups\20260319-160038\control-core\src\application`
- `D:\Projects\SiligenSuite\tmp\legacy-removal-backups\20260319-160038\control-core\modules\process-core`
- `D:\Projects\SiligenSuite\tmp\legacy-removal-backups\20260319-160038\control-core\modules\motion-core`

随后已完成物理删除。

## 6. 剩余高风险项

1. `control-core/src` 仍作为 shared compat include 根出现在 compile graph 中；这不是四个 legacy 目录的 direct build 依赖，但仍是 `control-core/src` 不能整体删除的高风险点。
2. standalone 测试仍真实依赖：
   - `control-core/src/shared`
   - `control-core/src/infrastructure/adapters/planning/dxf`
   这两个目录不在本轮删除范围，但说明 `control-core` 还不是可整体删除状态。
3. `modules/device-hal` 与 device/shared boundary 仍未收口，这也是删除后 `workspace-validation` / `ci` 中保留 `device-shared-boundary` 既有失败的原因。
4. shared include 大小写不一致告警仍存在，`shared/Geometry` 与 `shared/geometry` 在大小写敏感环境下仍有升级为真实问题的风险。

## 7. 当前结论与退出条件

当前结论：

1. `packages/process-runtime-core` 已经是 `control-core/src/domain`、`control-core/src/application`、`control-core/modules/process-core`、`control-core/modules/motion-core` 的真实构建 source root。
2. 这四个 legacy 目录已在 `2026-03-19` 物理删除，并受 `legacy-exit-check` 的缺失目录与 direct-path 归零门禁保护。
3. 删除后 `build.ps1`、`test.ps1`、`ci.ps1`、`protocol compatibility`、`engineering regression`、`simulated-line`、`hardware smoke`、`legacy-exit-check` 均已重跑；未出现新增回归。
4. 本轮没有把 runtime-host、transport-gateway、device 实现重新卷回 core，既有包边界保持不变。

退出条件：

1. 若要继续推进 `control-core/` 整目录删除，需要先迁出 `control-core/src/shared`、`control-core/src/infrastructure`、`control-core/modules/device-hal`、`control-core/third_party`。
2. 需要继续收口 control apps 顶层 CMake/source-root owner，避免整个工作区仍锚定 `control-core`。
3. 后续若要推进 `control-core/src` 物理瘦身，需要先把 shared compat include 根从 `control-core/src` 迁出或单独隔离。
