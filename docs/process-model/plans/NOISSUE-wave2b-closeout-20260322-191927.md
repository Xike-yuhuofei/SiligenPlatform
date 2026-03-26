# NOISSUE Wave 2B 总收尾

- 状态：Done
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-191927`
- 关联文档：
  - `docs/process-model/plans/NOISSUE-wave2b-batch3c-closeout-20260322-182913.md`
  - `docs/process-model/plans/NOISSUE-wave2b-batch4-closeout-20260322-190144.md`
  - `docs/process-model/plans/NOISSUE-wave2bD-integration-20260322-161904.md`
- 关联验证目录：`integration/reports/verify/wave2b-batch4/`

## 1. 总体裁决

1. `Wave 2B overall closeout = Done`
2. `Wave 2B overall acceptance = Go`
3. `Wave 3 planning entry = Go`
4. `Wave 3 implementation = No-Go`

## 2. 本次总收尾的核对范围

本次不依赖口头历史，只核对以下已落盘工件与当前工作区事实：

1. `NOISSUE-wave2b-batch4-closeout-20260322-190144.md`
2. `NOISSUE-wave2b-batch3c-closeout-20260322-182913.md`
3. `NOISSUE-wave2bD-integration-20260322-161904.md`
4. `integration/reports/verify/wave2b-batch4/` 下的 `packages/apps/integration/protocol/simulation` 报告
5. 当前 `git status --short`
6. Batch 4 终态关键文件的只读 spot-check

## 3. 已核对的关键事实

### 3.1 Batch 4 文档与目录存在性

1. `Wave 2B Batch 4` closeout 已落盘，且文档内部明确写有：
   - `Wave 2B Batch 4 = Done`
   - `Wave 2B = Done`
2. `Wave 2B Batch 3C` closeout 已落盘，且文档内部明确写有：
   - `Batch 4 = Go`
3. `Wave 2B-D integration` 设计整合文档已落盘，且文档内部明确写有：
   - `Wave 2B Design Freeze = Go`
   - `Wave 2B Physical Cutover = No-Go`
   - 后续执行顺序为 `Batch 0 -> 1 -> 2 -> 3 -> 4`
4. `integration/reports/verify/wave2b-batch4/` 目录已存在，且包含：
   - `packages/`
   - `apps/`
   - `integration/`
   - `protocol/`
   - `simulation/`

### 3.2 当前工作区对 Batch 4 终态的 spot-check

1. `cmake/workspace-layout.env`
   - 当前仅保留 13 个 canonical layout entry
   - 不再包含 `SILIGEN_SUPERBUILD_SOURCE_ROOT`
2. `tests/CMakeLists.txt`
   - 已改为通过 `cmake/LoadWorkspaceLayout.cmake` 加载 layout
   - `tests root` 必须解析为唯一 canonical superbuild source root
   - 未再保留 selector 分支
3. `tools/build/build-validation.ps1`
   - 当前仍保留 `CMAKE_HOME_DIRECTORY == repo root` 的 provenance gate
   - 不再输出 selector 口径
4. `config/machine_config.ini`
   - 当前工作区 `Test-Path` 结果为 `False`
   - root alias 实体文件已不存在
5. `config/machine/machine_config.ini`
   - 当前工作区 `Test-Path` 结果为 `True`
   - canonical machine config 仍为唯一有效路径
6. `packages/runtime-host/src/runtime/configuration/WorkspaceAssetPaths.h`
   - 显式旧路径请求返回 `compatibility-alias-not-supported`
   - 若现场残留 alias 文件，返回 `compatibility-alias-present`
   - 行为为 fail-fast，而非 warning-only bridge
7. `packages/runtime-host/src/runtime/recipes/TemplateFileRepository.cpp`
   - 若发现 legacy `templates.json`，直接返回错误
   - 不再做 merge/fallback
8. `packages/runtime-host/CMakeLists.txt`
   - 当前只保留 `siligen_runtime_recipe_persistence`
   - `siligen_device_hal_persistence` / `siligen_persistence` 在当前 CMake 扫描中已零命中
9. `tools/migration/wave2b_residual_ledger.json`
   - 当前为终态：`{"entries":[]}`
10. `tools/migration/validate_wave2b_residuals.py`
    - 当前仍要求 Batch 4 后 ledger 必须为空账本

## 4. Batch 4 报告复核结论

### 4.1 packages 报告

1. `passed=10 failed=0 known_failure=0 skipped=0`
2. 以下终态 gate 已落盘通过：
   - `workspace-layout-boundary`
   - `wave2b-residual-ledger`
   - `wave2b-residual-payoff`
   - `runtime-asset-cutover`
3. 关键指标：
   - `control_apps_cmake_home_directory = D:/Projects/SS-dispense-align`
   - `entry_count = 13`
   - `failed_provenance_count = 0`
   - `failed_wiring_count = 0`
   - `runtime-asset-cutover issue_count = 0`
   - `wave2b-residual-ledger entry_count = 0`
   - `wave2b-residual-payoff issue_count = 0`

### 4.2 apps 报告

1. `passed=23 failed=0 known_failure=0 skipped=0`
2. canonical 路径合同继续通过：
   - `control-runtime-subdir-canonical-config`
   - `control-tcp-server-subdir-canonical-config`
   - `control-cli-bootstrap-check-subdir`
3. compat alias 删除后的 hard-fail 合同继续通过：
   - `control-runtime-compat-alias-rejected`
   - `control-tcp-server-compat-alias-rejected`
   - `control-cli-bootstrap-check-compat-alias-rejected`
4. 说明 Batch 4 并未破坏 canonical 相对路径合同，同时旧 alias 已不再可用。

### 4.3 simulation 报告

1. `overall_status = passed`
2. `compat_rect_diag` 通过
3. 4 个 `scheme_c` 场景全部通过
4. `runtime_bridge_mode = process_runtime_core`
5. `process_runtime_core_linked = true`

### 4.4 integration / protocol 报告

1. `integration` 报告目录已落盘
2. `protocol` 报告目录已落盘
3. 与 Batch 4 closeout 中的根级 suite 全绿结论一致

## 5. 当前工作树裁决

1. 当前工作树明显不干净，且包含大量未提交改动与未跟踪文档。
2. 这不影响 `Wave 2B` 总收尾结论，因为：
   - Batch 4 closeout、验证目录和关键终态文件彼此一致
   - spot-check 未发现 Batch 4 终态被后续工作区内容破坏
3. 这会影响“是否可以再发起一轮新的 QA 回归”：
   - 按 `ss-qa-regression` 规则，脏工作树下不应直接开始新的 QA 执行
   - 因此本次总收尾以“已落盘证据 + 当前只读核对”为准，不重跑同一 build root 的根级验证

## 6. 为什么 Wave 2B 现在可以正式收口

1. `Wave 2B-D integration` 中定义的 `Batch 0 -> 4` 路径已经全部走完。
2. `Batch 3C` 已把 `must_zero_before_cutover=true` 的 4 个 blocking family 清零 live debt。
3. `Batch 4` 已完成 bridge/fallback 删除、compat alias 删除、residual ledger 终态清零和根级 suites 验证。
4. 当前工作区 spot-check 与 Batch 4 closeout 结论一致，没有发现“closeout 已写 Done，但现场已回退”的证据。

## 7. 下一阶段裁决

### 7.1 允许进入

1. `Wave 3 planning = Go`
2. 下一阶段应从新的 `scope / arch-plan / test-plan` 开始，主题应对齐：
   - `app boundary`
   - `gateway` 收口
   - `apps/* shell` 与内部 owner 的职责切分

### 7.2 当前不允许直接做的事

1. 不允许仅凭 `Wave 2B` 已完成就直接开始 `Wave 3` 实施。
2. 在新的 `Wave 3` scope、技术方案、测试计划和写集边界落盘前，不应直接修改 app boundary 或 gateway owner。
3. 在当前脏工作树上，不应补跑新的全量 QA 作为“进入 Wave 3 实施”的依据。

## 8. 最终结论

1. `Wave 2B` 已具备 authoritative 总收尾证据闭环。
2. 本轮最合理的后续阶段不是重做 `Batch 4`，也不是直接进入下一波次实施，而是：
   - 先以本文件作为 `Wave 2B` authoritative closeout
   - 再进入 `Wave 3` 的 scope / arch / test planning
