# ARCH-201 US4 Task 5 Evidence

## 范围结论

- 当前 US4 继续以交接文档中的已确认事实为准：`M8 dispense-packaging` 是 trigger/CMP 业务语义与 authority trigger layout 的唯一 owner，`M7 motion-planning` 只保留轨迹/数学 helper，`workflow` 只做 orchestration / consumer。
- `Task 5` 目标限定为补边界门禁与验证证据，不扩大到重写旧 preview residue 或重开 owner 迁移范围。

## 本轮新增门禁

- 在 [assert-module-boundary-bridges.ps1](/D:/Projects/wt-arch201-4/scripts/validation/assert-module-boundary-bridges.ps1) 中新增 3 条 US4 定向规则：
- 禁止 `modules/workflow/application/CMakeLists.txt` 重新编译 `services/dispensing/PlanningPreviewAssemblyService.cpp`
- 禁止 [PlanningPreviewAssemblyService.cpp](/D:/Projects/wt-arch201-4/modules/workflow/application/services/dispensing/PlanningPreviewAssemblyService.cpp) 重新直接包含 `DispensePlanningFacade`
- 禁止该 residue 文件重新调用 `AssemblePlanningArtifacts(...)` 在 workflow 本地重建 `M8` truth

## 本轮新增定向回归

- 在 [MotionPlanningOwnerBoundaryTest.cpp](/D:/Projects/wt-arch201-4/modules/motion-planning/tests/unit/domain/motion/MotionPlanningOwnerBoundaryTest.cpp) 新增 `WorkflowRetiredPreviewResidueCannotRebuildM8TruthLocally`
- 该测试锁定以下不变量：
- `workflow/application` 不得把 `PlanningPreviewAssemblyService.cpp` 重新纳入编译
- residue 文件必须保留 retired shim 标记
- residue 文件不得再出现 `DispensePlanningFacade` 或 `AssemblePlanningArtifacts(` 文本

## Residue 处理结果

- [PlanningPreviewAssemblyService.cpp](/D:/Projects/wt-arch201-4/modules/workflow/application/services/dispensing/PlanningPreviewAssemblyService.cpp) 已降级为显式退役 shim
- 当前行为改为直接返回失败，错误信息要求调用方改走 `PlanningUseCase authority/execution chain`
- 这样做的目的不是恢复该入口，而是避免后续有人把它重新接回 workflow 本地 owner concrete 时悄悄形成双真值

## 已执行验证

- 已执行边界脚本：
- [module-boundary-bridges.md](/D:/Projects/wt-arch201-4/tests/reports/module-boundary-bridges-us4-task5/module-boundary-bridges.md)
- 结果：`passed`
- 生成时间：`2026-04-04T17:21:06`
- finding_count：`0`

## 未执行与超范围说明

- 未执行 `ctest`
- 原因：当前 worktree 不存在 `build/` 目录，`D:/Projects/wt-arch201-4/build` 为缺失状态
- 因此本轮验证证据只包含源码级 boundary gate 与报告，不包含 C++ 二进制测试执行结果
- 这属于交接文档已知约束下的 out-of-scope 验证缺口，本轮不扩展到补建整个构建目录
