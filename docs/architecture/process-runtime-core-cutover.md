# process-runtime-core Cutover

更新时间：`2026-03-18`

## 1. 目标

本轮切换的目标是让 `packages/process-runtime-core` 承接真实核心实现，并切断默认构建对以下 legacy 路径的真实 owner 依赖：

- `control-core/src/domain`
- `control-core/src/application`
- `control-core/modules/process-core`
- `control-core/modules/motion-core`

约束保持不变：

- 不把 runtime 装配、TCP 协议、device 实现重新卷回 core
- 包内子域边界继续保持
- 优先切换真实构建和真实测试，而不是只做源码复制

## 2. 当前切换结果

### 2.1 核心源码承接

- 已把 `control-core/src/domain`、`control-core/src/application`、`control-core/modules/process-core`、`control-core/modules/motion-core` 的已编译源码承接到 `packages/process-runtime-core` 内部。
- 当前采用 package-local compatibility mirror 中间态：
  - `packages/process-runtime-core/src/domain`
  - `packages/process-runtime-core/src/application`
  - `packages/process-runtime-core/modules/process-core`
  - `packages/process-runtime-core/modules/motion-core`
- `process/`、`planning/`、`execution/`、`machine/`、`safety/`、`recipes/`、`dispensing/`、`diagnostics-domain/`、`application/` 继续作为逻辑 owner 和长期物理落点。

### 2.2 CMake 真实切换

- `packages/process-runtime-core/CMakeLists.txt` 已从 interface shell 改为真实 owner，直接编译 package 内的 `src/domain`、`src/application`、`modules/process-core`、`modules/motion-core`。
- `control-core/CMakeLists.txt` 已停止直接 `add_subdirectory(src/domain)` 和 `add_subdirectory(src/application)`，改为引入 `packages/process-runtime-core`。
- `control-core/modules/CMakeLists.txt` 已改为优先复用 package 侧已注册的 `siligen_process_core` / `siligen_motion_core`，避免重复编译 legacy module。
- `packages/simulation-engine/CMakeLists.txt` 已切到 package include 路径，不再直接 include `control-core/src/application`、`control-core/src/domain`、`control-core/modules/process-core/include`、`control-core/modules/motion-core/include`。

### 2.3 tests 真实切换

- `packages/process-runtime-core/tests/CMakeLists.txt` 现在是 `siligen_unit_tests` 与 `siligen_pr1_tests` 的真实定义点。
- `control-core/tests/CMakeLists.txt` 仅保留兼容转发。
- package 侧测试直接依赖 `siligen_process_runtime_core`，不再显式依赖 `../../control-core/...`。

### 2.4 include / link 清理

- `PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS` 以 package 内 `src`、`modules/process-core/include`、`modules/motion-core/include` 为优先入口。
- `packages/runtime-host`、`control-core/src/infrastructure` 等消费侧 include 顺序已调整为 package 优先，降低从 legacy 头路径回退解析的概率。
- `siligen_application_dispensing` 已去掉对 runtime 侧调度实现的直接 link，避免把宿主调度实现重新拉回 core。

### 2.5 README 与依赖规则同步

已同步以下文档/规则，明确 `packages/process-runtime-core` 是 canonical owner，legacy 路径只保留镜像/兼容语义：

- `packages/process-runtime-core/README.md`
- `packages/process-runtime-core/BOUNDARIES.md`
- `packages/process-runtime-core/tests/README.md`
- `packages/process-runtime-core/modules/motion-core/README.md`
- `packages/runtime-host/README.md`
- `control-core/README.md`
- `control-core/PROJECT_BOUNDARY_RULES.md`
- `control-core/apps/control-runtime/README.md`
- `control-core/apps/control-runtime/container/README.md`

## 3. 当前中间态说明

当前不是“package 薄壳 + legacy 真实现”模式，而是“package 真实构建 owner + package 内 compatibility mirror”模式：

- 构建、link 和测试都已经从 package 侧 target 出发
- 物理源码仍保留 mirror 目录，是为了先完成真实构建切换，避免一次性改爆 include 树
- 后续应继续把源码从 mirror 目录向 package 顶层子域目录收敛，而不是继续扩大 mirror 生命周期

## 4. 验证结果

### 4.1 configure

执行命令：

```powershell
cmake -S control-core -B tmp\process-runtime-core-cutover-build-clean -DSILIGEN_BUILD_TESTS=ON -DSILIGEN_BUILD_TARGET=tests -DSILIGEN_USE_PCH=OFF -DSILIGEN_ENABLE_CGAL=OFF
```

结果：

- configure / generate 成功
- `apps/control-cli` 因缺少 `main.cpp` 被显式跳过，不影响本次 cutover 验证

### 4.2 build

执行命令：

```powershell
cmake --build tmp\process-runtime-core-cutover-build-clean --target siligen_pr1_tests --config Debug -j 1 -- /v:m
cmake --build tmp\process-runtime-core-cutover-build-clean --target siligen_unit_tests --config Debug -j 1 -- /v:m
cmake --build tmp\process-runtime-core-cutover-build-clean --target siligen_runtime_host --config Debug -j 1 -- /v:m
```

结果：

- 三个 target 均构建成功
- 编译日志显示核心源码来自 `packages/process-runtime-core/src/domain`、`packages/process-runtime-core/src/application`、`packages/process-runtime-core/modules/process-core`、`packages/process-runtime-core/modules/motion-core`

### 4.3 test

执行命令：

```powershell
ctest --test-dir tmp\process-runtime-core-cutover-build-clean -C Debug -R siligen_pr1_tests --output-on-failure
ctest --test-dir tmp\process-runtime-core-cutover-build-clean -C Debug -R "siligen_(unit|pr1)_tests" --output-on-failure
```

结果：

- `siligen_unit_tests` 通过
- `siligen_pr1_tests` 通过

### 4.4 simulation-engine 侧验证

执行命令：

```powershell
cmake -S packages/simulation-engine -B tmp\simulation-engine-process-runtime-core-cutover -DSIM_ENGINE_BUILD_EXAMPLES=OFF -DSIM_ENGINE_BUILD_TESTS=ON
cmake --build tmp\simulation-engine-process-runtime-core-cutover --target simulation_engine_scheme_c_runtime_bridge_shim_test --config Debug -j 1
ctest --test-dir tmp\simulation-engine-process-runtime-core-cutover -C Debug -R simulation_engine_scheme_c_runtime_bridge_shim_test --output-on-failure
```

结果：

- configure 成功
- `simulation_engine_scheme_c_runtime_bridge_shim_test` 构建成功
- `simulation_engine_scheme_c_runtime_bridge_shim_test` 通过

### 4.5 额外说明

- 旧 build 目录里曾出现过一次 `libprotoc` 文件锁导致的构建失败；换新 build 目录后已复现通过，判定为环境并发/锁问题，不是本次切换逻辑错误。

## 5. 当前仍阻止删除 legacy 的具体点

### 5.1 仍阻止删除 `control-core/src/*` 的点

1. `control-core/src/shared/*` 仍是 shared types、logging、PCH 和通用接口的真实来源；package 与 host/infrastructure 当前仍通过 `control-core/src` 获取 `shared/*`。
2. `control-core/src/infrastructure/*` 仍在真实编译，配置、存储、工厂、解析适配和部分 runtime infrastructure 仍由这里提供。
3. `control-core/src/adapters/tcp/*` 仍是 TCP 后端真实实现的一部分，不属于本次 core cutover 范围。
4. 对 `control-core/src/domain/*` 与 `control-core/src/application/*`，默认构建 owner 已切走，但物理删除前仍需要补一轮 header provenance 审计，确认没有消费者经由 `control-core/src` 兼容入口回退命中 legacy 头文件。
5. 当前活跃 README/规则已基本切到 package owner 语义；剩余文本残留主要在归档文档和少量兼容说明中，这不是默认构建 blocker，但在目录真正删除前仍需要继续清理。

### 5.2 仍阻止删除 `control-core/modules/process-core` / `control-core/modules/motion-core` 的点

1. 默认构建已经不再从这两个 legacy module 目录注册真实 target；owner 已切到 `packages/process-runtime-core/modules/*`。
2. 物理删除前仍需要补一轮 workspace 级引用审计，确认没有脚本、IDE 配置或未覆盖的 package 继续引用 legacy module 路径。
3. 当前剩余阻塞更多是“删除证明”而非“默认构建 owner”问题；如果直接删目录，需要同时清掉残留文本说明和兼容引用。

## 6. 剩余高风险点

1. 目前仍依赖 include 顺序让 `packages/process-runtime-core/src/*` 优先于 `control-core/src/*`。在没有完成 header provenance 审计前，不能宣称 legacy `src/domain` / `src/application` 已可物理删除。
2. package 当前仍处于 compatibility mirror 中间态；后续如果继续在 mirror 层堆新代码，会延长删除 legacy 的时间窗口。
3. `control-core` 作为整体仍承载 runtime / TCP / device / infrastructure 真实产物，因此 `control-core` 顶层目录距离整体删除还很远；本轮只完成了核心内核 owner 切换。

## 7. 下一步建议

1. 对 `control-core/src/domain`、`control-core/src/application`、`control-core/modules/process-core`、`control-core/modules/motion-core` 做一次 workspace 级 header / script provenance 审计，拿到“无真实命中”的删除证据。
2. 开始把 package 内 compatibility mirror 的源码逐步收敛到顶层逻辑子域目录，避免 package 长期停留在 mirror 形态。
3. 把 `packages/test-kit`、运行脚本和剩余文档中的 legacy 构建路径引用继续清零，为后续 legacy 删除门禁提供闭环证据。
