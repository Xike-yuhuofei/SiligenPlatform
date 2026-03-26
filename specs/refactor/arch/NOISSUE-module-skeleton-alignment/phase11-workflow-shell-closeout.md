# Phase 11: workflow shell-only bridge closeout

更新时间：`2026-03-25`

## 目标

将 `workflow` 从：

- canonical public surface owner-backed + internal impl bridge-backed

推进到：

- canonical-only implementation + shell-only compatibility bridges

本阶段目标仅覆盖 `workflow`，不宣称所有高复杂模块都已完成 hard closeout。

## 实施内容

### 1. canonical implementation roots 固化

本阶段确认 `workflow` 的唯一真实实现承载面为：

- `modules/workflow/domain`
- `modules/workflow/application`
- `modules/workflow/adapters`
- `modules/workflow/tests`

同时完成以下构建收口：

- `modules/workflow/domain/CMakeLists.txt`
  - `domain/domain` 作为 canonical 子域 CMake 入口，不再写入 `bridge-domain` 产物目录。
- `modules/workflow/CMakeLists.txt`
  - 实现 include roots 显式加入 `modules/workflow`、`domain/`、`application/`、`adapters/`。
- `modules/workflow/process-runtime-core/CMakeLists.txt`
  - 兼容 target 改为完全从 canonical `domain/`、`application/`、`adapters/`、`tests/` 组装。
- `modules/workflow/tests/process-runtime-core/CMakeLists.txt`
  - 测试 include roots 切到 canonical roots，不再依赖历史 bridge payload。

### 2. workflow 下游活跃依赖退出 bridge roots

为保证 `workflow/process-runtime-core/src` 可以物理排空，本阶段同步去除了活跃消费者对该根的显式依赖：

- `modules/dxf-geometry/adapters/dxf/CMakeLists.txt`
- `modules/dxf-geometry/src/dxf/CMakeLists.txt`

上述 DXF adapter 构建入口已改为消费 `modules/workflow/domain/include`，不再把 `workflow/process-runtime-core/src` 当作域头根。

### 3. bridge payload 物理排空

本阶段已物理删除：

- `modules/workflow/src/application/**`
- `modules/workflow/process-runtime-core/src/**`
- `modules/workflow/process-runtime-core/modules/**`

并进一步删除仅剩历史 README 的桥接分区目录：

- `modules/workflow/process-runtime-core/application`
- `modules/workflow/process-runtime-core/diagnostics-domain`
- `modules/workflow/process-runtime-core/dispensing`
- `modules/workflow/process-runtime-core/execution`
- `modules/workflow/process-runtime-core/machine`
- `modules/workflow/process-runtime-core/planning`
- `modules/workflow/process-runtime-core/process`
- `modules/workflow/process-runtime-core/recipes`
- `modules/workflow/process-runtime-core/safety`

因此，`workflow` 两个 bridge roots 当前已收敛为 shell-only 形态：

- `modules/workflow/src`
  - `README.md`
- `modules/workflow/process-runtime-core`
  - `CMakeLists.txt`
  - `README.md`
  - `BOUNDARIES.md`
  - `docs/**`
  - `tests/CMakeLists.txt`
  - `tests/README.md`

### 4. validator 与状态文档更新

本阶段同步增强：

- `scripts/migration/validate_workspace_layout.py`
  - 新增 `workflow` strict bridge contents 断言
  - 新增 `workflow` active forbidden snippets 断言
  - 新增对 `tests/CMakeLists.txt` canonical tests root 的硬性检查
- `modules/workflow/README.md`
- `modules/workflow/module.yaml`
- `modules/workflow/process-runtime-core/README.md`
- `modules/workflow/process-runtime-core/tests/README.md`
- `modules/workflow/tests/process-runtime-core/README.md`
- `specs/refactor/arch/NOISSUE-module-skeleton-alignment/README.md`
- `specs/refactor/arch/NOISSUE-module-skeleton-alignment/module-checklist.md`
- `specs/refactor/arch/NOISSUE-module-skeleton-alignment/wave-mapping.md`

## 验证结果

`2026-03-25` 已完成以下验证：

1. `.\build.ps1 -Profile Local -Suite contracts`
   - 通过
2. `python scripts/migration/validate_workspace_layout.py --wave "Wave 11"`
   - 通过
3. `cmake --build C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build --config Debug --target siligen_unit_tests siligen_pr1_tests siligen_runtime_host_unit_tests siligen_planner_cli siligen_transport_gateway`
   - 通过
4. `ctest --test-dir C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build -C Debug -R "siligen_runtime_host_unit_tests|siligen_unit_tests|siligen_pr1_tests" --output-on-failure`
   - 通过

## 关闭口径

本阶段完成后，`workflow` 的正确口径为：

- canonical implementation roots 是唯一真实实现面
- `src` 与 `process-runtime-core` 已降为 shell-only compatibility bridges
- `workflow` 已达到 `canonical-only implementation; shell-only bridges`

系统整体当前正确口径为：

- `runtime-execution` 已 `canonical-only implementation; shell-only bridges`
- `workflow` 已 `canonical-only implementation; shell-only bridges`
- 其余模块仍以既定 bridge-backed closeout 口径管理

因此，仍不得把系统整体表述为“所有模块都已完成 hard closeout”。
