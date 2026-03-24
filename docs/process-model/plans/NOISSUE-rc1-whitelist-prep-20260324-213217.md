# RC1 Whitelist Prep

## 1. 目标

为 `0.1.0-rc.1` 仓内候选版收口准备一份最小白名单范围，避免在当前脏工作树条件下直接进行盲提交。

## 2. 当前候选范围

### 2.1 本轮直接产出

1. [`CHANGELOG.md`](/D:/Projects/SS-dispense-align/CHANGELOG.md)
2. [`docs/architecture/workspace-baseline.md`](/D:/Projects/SS-dispense-align/docs/architecture/workspace-baseline.md)
3. [`docs/process-model/plans/NOISSUE-rc1-release-scope-20260324-213217.md`](/D:/Projects/SS-dispense-align/docs/process-model/plans/NOISSUE-rc1-release-scope-20260324-213217.md)
4. [`docs/process-model/reviews/refactor-arch-NOISSUE-architecture-refactor-spec-premerge-20260324-213217.md`](/D:/Projects/SS-dispense-align/docs/process-model/reviews/refactor-arch-NOISSUE-architecture-refactor-spec-premerge-20260324-213217.md)
5. [`docs/process-model/reviews/refactor-arch-NOISSUE-architecture-refactor-spec-qa-20260324-213217.md`](/D:/Projects/SS-dispense-align/docs/process-model/reviews/refactor-arch-NOISSUE-architecture-refactor-spec-qa-20260324-213217.md)
6. [`docs/process-model/reviews/refactor-arch-NOISSUE-architecture-refactor-spec-release-20260324-213217.md`](/D:/Projects/SS-dispense-align/docs/process-model/reviews/refactor-arch-NOISSUE-architecture-refactor-spec-release-20260324-213217.md)

### 2.2 需要特别处理的文件

1. [`ci.ps1`](/D:/Projects/SS-dispense-align/ci.ps1)
   - 当前状态：`MM`
   - 含义：该文件在本轮之前已经存在 staged 变更；本轮又追加了未暂存修复
   - 本轮新增修复内容：
     - 统一解析绝对/相对 `ReportDir`
     - 将 `legacy-exit` 与 `test.ps1` 的报告目录对齐到解析后的绝对路径
   - 风险：若直接整体提交，可能把本轮之外的 staged 变更一并带入基线提交

## 3. 当前 Git 观察

1. 直接查看本轮候选文件的工作树状态：
   - `M CHANGELOG.md`
   - `MM ci.ps1`
   - `M docs/architecture/workspace-baseline.md`
   - `?? docs/process-model/plans/NOISSUE-rc1-release-scope-20260324-213217.md`
   - `?? docs/process-model/reviews/refactor-arch-NOISSUE-architecture-refactor-spec-premerge-20260324-213217.md`
   - `?? docs/process-model/reviews/refactor-arch-NOISSUE-architecture-refactor-spec-qa-20260324-213217.md`
   - `?? docs/process-model/reviews/refactor-arch-NOISSUE-architecture-refactor-spec-release-20260324-213217.md`
2. 相对 `HEAD` 的 tracked diff 仅明确覆盖：
   - `CHANGELOG.md`
   - `ci.ps1`
   - `docs/architecture/workspace-baseline.md`
3. 其中 `ci.ps1` 的 cached diff 明确表明：该文件在本轮前已包含 canonical CI 链接线改动，不能假定整个文件都属于本轮白名单。
4. 当前 index 已存在 `22` 个 staged 文件，不只包含本轮 RC 文档：
   - `tools/`：`5`
   - `shared/`：`4`
   - `scripts/`：`3`
   - `cmake/`：`2`
   - `docs/`：`2`
   - `specs/`：`2`
   - 根级：`.gitignore`、`build.ps1`、`ci.ps1`、`test.ps1`
5. 这意味着“文档白名单逻辑上已成立”，但“当前 git index 还不能直接用普通 `git commit` 形成该白名单提交”。

## 4. 建议的收口策略

1. 若目标是“最小 RC 文档基线提交”，优先只收口以下文件：
   - `CHANGELOG.md`
   - `docs/architecture/workspace-baseline.md`
   - `docs/process-model/plans/NOISSUE-rc1-release-scope-20260324-213217.md`
   - `docs/process-model/reviews/refactor-arch-NOISSUE-architecture-refactor-spec-premerge-20260324-213217.md`
   - `docs/process-model/reviews/refactor-arch-NOISSUE-architecture-refactor-spec-qa-20260324-213217.md`
   - `docs/process-model/reviews/refactor-arch-NOISSUE-architecture-refactor-spec-release-20260324-213217.md`
2. 若目标是“让 CI 入口修复也随基线提交入库”，必须先对 `ci.ps1` 的 staged 与 unstaged 组合状态做一次显式复核，再决定是否整体纳入同一提交。
3. 在未拆清 `ci.ps1` 之前，不建议直接执行基线提交。

## 5. 当前结论

1. 文档白名单已经可独立收口。
2. `ci.ps1` 需要单独做 staged/unstaged 边界复核。
3. 当前 index 已混入非白名单 staged 文件，因此在不做 index 级隔离之前，不建议直接执行普通 `git commit`。
4. 下一步最合理的动作是：先决定是否允许做 index 级隔离；若允许，再把 RC 文档白名单从现有 staged 集合中拆出来单独提交。
