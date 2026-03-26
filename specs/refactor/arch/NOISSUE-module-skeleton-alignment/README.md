# 模块统一骨架对齐工作目录

本目录用于集中保存本次会话形成的主要工作材料，便于后续与实际迁移结果做对照。

## 目录说明

- `spec.md`：本轮需求理解、冻结目标、前提假设与边界条件
- `design.md`：候选方案对比、推荐方案与选择理由
- `plan.md`：已确认推荐方案下的总体迁移蓝图
- `motion-planning-preview.md`：`modules/motion-planning` 的统一骨架目标预览
- `module-checklist.md`：模块级完成矩阵与 closeout 判定
- `wave-mapping.md`：迁移波次、进入/退出条件与当前状态
- `phase7-closeout.md`：Phase 7 收尾证据与剩余 bridge 风险登记
- `phase8-runtime-execution-bridge-drain.md`：`runtime-execution` canonical-first build cutover 与 bridge-drain 起步记录
- `phase9-workflow-public-surface-ownerization.md`：`workflow` canonical public surface owner 化与下游消费链验证记录
- `phase10-runtime-execution-shell-closeout.md`：`runtime-execution` shell-only bridge closeout 与最小 workflow 配套清障记录
- `phase11-workflow-shell-closeout.md`：`workflow` canonical-only implementation + shell-only bridge closeout 记录
- `phase12-low-risk-shell-closeout.md`：`trace-diagnostics`、`topology-feature`、`coordinate-alignment` 的低风险 shell-only closeout，与 `process-planning` closeout-ready 清障记录
- `phase13-planning-chain-shell-closeout.md`：`process-planning`、`process-path`、`dispense-packaging` 的规划链 shell-only closeout，以及 `motion-planning` residual freeze 记录
- `phase14-motion-planning-owner-activation.md`：`motion-planning` canonical owner target 激活、`workflow` supporting target 预加载，以及 canonical tests 登记加固记录

## 当前结论

- 本轮目标不是新增模块 root，而是把 `modules/` 下所有一级模块从“owner 入口已落位”推进到“模块内部统一骨架已落位”。
- 当前采用的总体方向为：统一骨架先落位、旧结构保留为迁移桥、按模块逐步收敛。
- 当前 `Phase 1-7` 已按 `bridge-backed closeout` 口径完成：统一骨架、模块元数据、bridge 标记和 build-switch 元数据已落位。
- 当前状态不等价于“所有历史 bridge 目录已物理清空”；高复杂模块仍保留兼容桥，后续如需硬切换，应以新 Phase 单独推进。
- `Phase 8` 已开始处理高复杂模块的 bridge drain；当前优先落地的是 `runtime-execution` 的 canonical-first build roots。
- `Phase 9` 已完成 `workflow` canonical public surface owner 化，并确认 `planner-cli`、`transport-gateway` 等下游 target 可通过新的 public surface 闭环构建。
- `Phase 10` 已把 `runtime-execution/device-contracts`、`device-adapters`、`runtime-host` 收敛为 shell-only bridge roots，并移除 canonical CMake 对 bridge vendor 的默认回退。
- `Phase 11` 已把 `workflow/src` 与 `workflow/process-runtime-core` 收敛为 shell-only bridge roots；`workflow` 现已达到 `canonical-only implementation + shell-only bridges`。
- `Phase 12` 已把 `trace-diagnostics/src`、`topology-feature/src`、`coordinate-alignment/src` 收敛为 shell-only bridge roots，并为 `process-planning` 落地 canonical `domain/configuration` target 与 closeout-ready 校验。
- `Phase 13` 已把 `process-planning/src`、`process-path/src`、`dispense-packaging/src` 收敛为 shell-only bridge roots，并把 `workflow` planning/dispensing interface target 切到新的模块 canonical target 转发。
- `Phase 14` 已激活 `motion-planning` canonical owner target，并修复 `workflow` canonical tests 的 supporting target / 测试登记链。
- `motion-planning` 在 `Phase 14` 后的正确口径是 `bridge-backed; canonical owner target active; residual runtime/control surface frozen`。
