# Tasks: ARCH-201 Stage D - Planning Owner Hard Cut

- [X] T001 删除 `workflow` 下的 planning concrete / public header residual
- [X] T002 删除 dead `PlanningPreviewAssemblyService`
- [X] T003 将 owner boundary tests 改为断言 workflow planning residual 不存在
- [X] T004 将 `assert-module-boundary-bridges.ps1` 升级为对 residual 文件存在性硬失败
- [X] T005 更新 `workflow` / `dispense-packaging` README 与 Stage D 规格文档
- [X] T006 执行 Stage D 第一批定向 build/test 与边界脚本
- [X] T007 扫描仓库中对 removed workflow planning artifacts 的现行 consumer / docs / index 残留
- [X] T008 清理 `modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md` 中的 workflow planning residual 文件树与实现说明残留，并校正统计计数
- [X] T009 更新 Stage D traceability 记录第二批 docs / evidence tightening 的 repo scan 结论
- [X] T010 为 `AuthorityPreviewAssemblyService` / `ExecutionAssemblyService` 补 direct owner-side tests，锁定 split assembly public seam
- [X] T011 为 `PlanningUseCase` 补 split seam parity test，显式锁定 `PrepareAuthorityPreview + AssembleExecutionFromAuthority == Execute`
- [X] T012 补齐 `MotionPlanningOwnerBoundaryTest` 的 hard-cut removed artifact 清单，与 boundary script 保持 1:1
- [X] T013 修正 `workflow` 现行基线文档中的 live seam 真值，去除对 `DispensePlanningFacade` 作为 workflow direct seam 的错误描述
- [X] T014 执行 Stage D 收口定向 build/test 与边界脚本，准备将 PR #95 从 draft 转 ready
