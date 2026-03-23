# 事件描述

- 现象：用户已删除 `ss-workflow` 整套工作流，但工作区中再次出现 `.agents/skills/ss-workflow/SKILL.md`、`.agents/skills/README.md` 和 `docs/process-model/codex-workflow.md`。
- 调查目标：确认这些文件是被脚本自动恢复，还是被 Git/提交历史重新检出。

# 复现步骤

1. 检查当前工作区状态：`git status --short --branch`
2. 检查 `ss-workflow` 当前是否受 Git 跟踪：`git log --follow --name-status -- .agents/skills/ss-workflow/SKILL.md`
3. 检查新增与删除历史：
   - `git log --all --diff-filter=A --name-status -- .agents/skills/ss-workflow/SKILL.md .agents/skills/README.md docs/process-model/codex-workflow.md`
   - `git log --all --diff-filter=D --name-status -- .agents/skills/ss-workflow/SKILL.md .agents/skills/README.md docs/process-model/codex-workflow.md`
4. 检查是否存在仓内自动恢复脚本：`rg -n --hidden --glob '!**/.git/**' "\.agents/skills|ss-workflow|codex-workflow|SKILL.md" tools .github .agents docs`
5. 检查 Git hook：`git config --show-origin --get core.hooksPath` 以及公共 Git 目录 `D:/Projects/SiligenSuite/.git/hooks`
6. 检查文件落盘时间与 reflog：`Get-Item ... | Select CreationTime,LastWriteTime`、`git reflog --date=iso --all --since="2026-03-22 11:50" --until="2026-03-22 12:20"`

# 根因分析

## 直接根因

- `ss-workflow` 不是运行时自动生成的，而是被明确提交回仓库。
- 证据：提交 `7276ad75b7425b12cdf8a8fb9f2c53598ff6ba7b` `fix(runtime): align dxf preview contract semantics and workflow evidence` 新增了以下受管文件：
  - `.agents/skills/README.md`
  - `.agents/skills/ss-workflow/SKILL.md`
  - `docs/process-model/codex-workflow.md`
  - 同时修改了 `AGENTS.md`
- 针对这三个新增文件，`git log --all --diff-filter=D` 未发现后续删除提交。这意味着从 Git 历史看，它们自该提交后一直存在于分支祖先中。

## 为什么会在当前工作区“恢复”

- 当前分支 `feat/dispense/NOISSUE-e2e-flow-spec-alignment` 是在 `2026-03-22 12:08:00 +0800` 从 `HEAD` 创建的。
- reflog 显示该时刻紧接着发生了 `reset: moving to HEAD`。
- 三个文件在当前工作区的 `CreationTime/LastWriteTime` 都是 `2026-03-22 12:08:01`，与该分支创建/重置时间吻合。
- 结论：Git 在分支创建/重置时把 `HEAD` 中已跟踪的 `ss-workflow` 相关文件重新检出了工作区。它们“恢复”的原因是当前 `HEAD` 本来就包含这些文件，而不是某个脚本在后台重新生成。

## 排除项

- 仓内未发现会复制、模板化生成或自动重建 `ss-workflow` 的脚本。
- 公共 Git hooks 目录只有 sample 文件，没有启用的 `post-checkout`、`post-merge`、`post-commit` hook。
- `git diff 7276ad75..HEAD -- .agents/skills/ss-workflow/SKILL.md .agents/skills/README.md docs/process-model/codex-workflow.md AGENTS.md` 为空，说明这些文件在加入后未被后续提交再次改写；当前内容本质上就是那次提交留下的版本。

# 修复说明

- 调查阶段先只定位原因；后续已按同一证据链执行永久删除，删除对象与入口清理范围保持一致。
- 如果目标是永久移除 `ss-workflow`，必须提交一次真正的删除变更，并同步清理以下引用：
  - `AGENTS.md`
  - `.agents/skills/README.md`
  - `docs/process-model/codex-workflow.md`
- 仅在工作区手动删除文件但不提交，后续任何 checkout / reset / branch create / merge / pull 都可能把这些受管文件重新写回。

# 回归验证

- 已验证 `ss-workflow` 当前位于 Git 跟踪集内。
- 已验证存在明确的“重新加入仓库”提交。
- 已验证不存在后续“删除该文件”的提交。
- 已验证仓内脚本和 Git hook 不构成自动恢复路径。

# 防复发措施

1. 若确认废弃该工作流，提交一次完整删除变更，而不是只删工作区文件。
2. 同一次提交内同步删除/修改 `AGENTS.md`、`.agents/skills/README.md`、`docs/process-model/codex-workflow.md` 中的入口说明，避免后续人工或代理再次按文档补回。
3. 在删除后执行 `git log --all -- .agents/skills/ss-workflow/SKILL.md` 和 `git grep "ss-workflow"` 作为退出检查，确认历史新增之后已经有新的删除提交且引用已清空。
