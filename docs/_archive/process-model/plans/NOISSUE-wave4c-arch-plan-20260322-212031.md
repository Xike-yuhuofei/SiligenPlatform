# NOISSUE Wave 4C Architecture Plan - External Observation Evidence Pack

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-212031`
- 关联 scope：`docs/process-model/plans/NOISSUE-wave4c-scope-20260322-212031.md`

## 1. 目标结构

```text
docs/runtime/external-migration-observation.md
  -> observation target list
  -> Go / No-Go / input-gap policy

integration/reports/verify/wave4c-closeout
  -> legacy-exit-pre/
  -> observation/
     -> external-launcher.md
     -> field-scripts.md
     -> release-package.md
     -> rollback-package.md
     -> summary.md
     -> blockers.md
  -> legacy-exit-post/
```

## 2. 组件职责

### 2.1 观察文档

1. `external-migration-observation.md` 继续作为唯一执行入口。
2. 本阶段把证据目录从 `wave4b-closeout` 切到 `wave4c-closeout`。

### 2.2 观察报告

1. `external-launcher.md`
   - 记录当前可访问环境对 canonical / legacy launcher 命令的可见性。
2. `field-scripts.md`
   - 记录现场脚本快照是否存在，以及缺失时的 `input-gap`。
3. `release-package.md`
   - 记录发布包或解包根是否存在，以及扫描结果或缺失阻断。
4. `rollback-package.md`
   - 记录回滚包 / SOP 是否存在，以及扫描结果或缺失阻断。
5. `summary.md`
   - 只汇总四类对象的结论、证据路径和下一动作。
6. `blockers.md`
   - 只保留阻断项，不混入通过项。

## 3. 失败模式与补救

1. 失败模式：canonical launcher 在当前外部环境不可见
   - 补救：判定 `No-Go`，要求补 canonical 安装面，再重验。
2. 失败模式：缺少发布包、回滚包或现场脚本快照
   - 补救：判定 `input-gap`，要求补输入，不做推定通过。
3. 失败模式：观察中命中 legacy 入口
   - 补救：只允许替换仓外交付物并重验，不允许恢复 compat shell。

## 4. 回滚边界

1. 本阶段只新增/修改文档与证据工件。
2. 若需回退，只回退 `Wave 4C` 工件，不触碰产品代码或 `Wave 4A / 4B` 实现。
