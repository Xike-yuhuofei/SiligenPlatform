# NOISSUE Wave 4E Test Plan - Intake Preparation Verification

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-215937`
- 关联 scope：`docs/process-model/plans/NOISSUE-wave4e-scope-20260322-215937.md`
- 关联 arch：`docs/process-model/plans/NOISSUE-wave4e-arch-plan-20260322-215937.md`

## 1. 验证原则

1. 只验证准备工件，不伪造外部输入，不补跑仓外观察结论。
2. 不跑新的全量 build/test；只做文档与目录一致性校验。
3. 所有校验命令都必须是只读命令。

## 2. 目录与模板校验

```powershell
Test-Path .\integration\reports\verify\wave4e-rerun
Test-Path .\integration\reports\verify\wave4e-rerun\intake
Test-Path .\integration\reports\verify\wave4e-rerun\observation
rg -n "pending-input|Recommended Commands|wave4e-rerun" docs/runtime/external-migration-observation.md integration/reports/verify/wave4d-closeout/intake integration/reports/verify/wave4e-rerun
```

预期：

1. 新目录全部存在。
2. 观察期文档不再把新一轮证据硬编码到 `wave4c-closeout/observation/`。
3. intake 模板包含可执行采集命令和 `pending-input` 语义。

## 3. 历史结论保护

```powershell
rg -n "Wave 4C|Wave 4D|No-Go|Go" docs/process-model/plans/NOISSUE-wave4e-scope-20260322-215937.md docs/process-model/plans/NOISSUE-wave4e-arch-plan-20260322-215937.md docs/process-model/plans/NOISSUE-wave4e-test-plan-20260322-215937.md
```

预期：

1. 只允许引用历史结论作为边界，不允许把本阶段写成新的 closeout。
2. 不允许出现“仓外迁移完成”之类超出当前准备阶段的结论。
