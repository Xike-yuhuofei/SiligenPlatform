# Phase 12: low-risk shell-only closeout

更新时间：`2026-03-25`

## 目标

在不扩大到宿主/CLI 拆分与大规模规划链连续 drain 的前提下，完成一批低风险模块的 shell-only closeout，并为下一阶段规划链收口落地前置条件。

本阶段目标分为两类：

- `trace-diagnostics`
- `topology-feature`
- `coordinate-alignment`

推进到：

- `canonical-only implementation; shell-only bridges`

同时把 `process-planning` 推进到：

- `bridge-backed; closeout-ready preconditions landed`

## 实施内容

### 1. topology-feature canonical target 固化

本阶段为 `modules/topology-feature/domain/geometry` 建立独立 canonical target：

- `siligen_topology_feature_contour_augment_adapter`

并同步完成：

- `modules/topology-feature/CMakeLists.txt`
  - 模块根 target 改为直接链接 `siligen_topology_feature_contour_augment_adapter`
  - 删除对 `siligen_process_runtime_core_planning` 的 fallback
- `modules/topology-feature/domain/geometry/CMakeLists.txt`
  - 删除 `${SILIGEN_WORKFLOW_CORE_DIR}/src`
  - 不再复用 `siligen_contour_augment_adapter` 名称，避免与 `workflow` owner target 混淆

### 2. coordinate-alignment canonical target 固化

本阶段为 `modules/coordinate-alignment/domain/machine` 建立独立 canonical target：

- `siligen_coordinate_alignment_domain_machine`

并同步完成：

- `modules/coordinate-alignment/CMakeLists.txt`
  - 模块根 target 改为直接链接 `siligen_coordinate_alignment_domain_machine`
  - 删除对 `siligen_process_runtime_core_planning` 与 `siligen_process_runtime_core_machine` 的 fallback
- `modules/coordinate-alignment/domain/machine/CMakeLists.txt`
  - 用 `SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR` 与 `SILIGEN_SHARED_COMPAT_INCLUDE_ROOT` 替代 `${PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS}`

因此，`coordinate-alignment` 对 `domain/diagnostics/*`、`domain/motion/*` 的依赖已统一经 `workflow` canonical public headers 解析。

### 3. trace-diagnostics bridge payload 物理排空

`trace-diagnostics` 的真实实现仍由 canonical `adapters/logging/spdlog` 承载。

本阶段已物理删除：

- `modules/trace-diagnostics/src/logging/spdlog/**`

因此，`modules/trace-diagnostics/src` 当前仅剩：

- `README.md`

### 4. topology-feature / coordinate-alignment bridge payload 物理排空

本阶段已物理删除：

- `modules/topology-feature/src/geometry/**`
- `modules/coordinate-alignment/src/machine/**`

因此，以下 bridge roots 当前均已收敛为 shell-only 形态：

- `modules/topology-feature/src`
  - `README.md`
- `modules/coordinate-alignment/src`
  - `README.md`

### 5. process-planning closeout-ready 清障

本阶段不宣称 `process-planning` 已完成 shell-only closeout，但已完成下一阶段连续 drain 所需的前置清障：

- `modules/process-planning/domain/configuration/CMakeLists.txt`
  - 从占位文件提升为 canonical target：`siligen_process_planning_domain_configuration`
- `modules/process-planning/CMakeLists.txt`
  - 模块根 target 改为直接链接 `siligen_process_planning_domain_configuration`
- `modules/process-planning/domain/configuration/value-objects/ConfigTypes.h`
  - 删除对 `../../motion/...`、`../../safety/...` 的历史相对路径引用
  - 改为消费 `workflow` canonical public headers

当前正确口径为：

- `process-planning` 仍是 `bridge-backed`
- 但模块根 target 已不再默认依赖 `siligen_process_runtime_core_planning`

### 6. validator 与状态文档更新

本阶段同步增强：

- `scripts/migration/validate_workspace_layout.py`
  - 新增 `Phase 12` strict bridge contents 断言
  - 新增 `topology-feature`、`coordinate-alignment` 的 canonical target / forbidden snippets 断言
  - 新增 `process-planning` closeout-ready required / forbidden snippets 断言
- `modules/topology-feature/README.md`
- `modules/topology-feature/module.yaml`
- `modules/coordinate-alignment/README.md`
- `modules/coordinate-alignment/module.yaml`
- `modules/trace-diagnostics/README.md`
- `modules/trace-diagnostics/module.yaml`
- `modules/process-planning/README.md`
- `modules/process-planning/module.yaml`
- `specs/refactor/arch/NOISSUE-module-skeleton-alignment/README.md`
- `specs/refactor/arch/NOISSUE-module-skeleton-alignment/module-checklist.md`
- `specs/refactor/arch/NOISSUE-module-skeleton-alignment/wave-mapping.md`

## 验证结果

`2026-03-25` 已完成以下验证：

1. `python scripts/migration/validate_workspace_layout.py --wave "Wave 12"`
   - 通过
2. `.\build.ps1 -Profile Local -Suite contracts`
   - 通过
3. `cmake --build C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build --config Debug --target siligen_runtime_host_unit_tests siligen_planner_cli siligen_unit_tests`
   - 通过
4. `ctest --test-dir C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build -C Debug -R "siligen_runtime_host_unit_tests|siligen_unit_tests" --output-on-failure`
   - 通过
5. `cmake --build C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build --config Debug --target siligen_topology_feature_contour_augment_adapter siligen_coordinate_alignment_domain_machine siligen_process_planning_domain_configuration siligen_spdlog_adapter`
   - 通过

## 关闭口径

本阶段完成后，模块级正确口径为：

- `runtime-execution` 已 `canonical-only implementation; shell-only bridges`
- `workflow` 已 `canonical-only implementation; shell-only bridges`
- `trace-diagnostics` 已 `canonical-only implementation; shell-only bridges`
- `topology-feature` 已 `canonical-only implementation; shell-only bridges`
- `coordinate-alignment` 已 `canonical-only implementation; shell-only bridges`
- `process-planning` 仍为 `bridge-backed; closeout-ready preconditions landed`

系统整体仍不得表述为“所有模块都已完成 hard closeout”。
