---
name: fresh-session-handoff
description: Summarize the current Codex window into a factual handoff note and launch a fresh Codex session in a Windows Terminal tab or window rooted at the current worktree. Use when the user explicitly wants to switch to a clean session, continue long-running work in a new terminal surface, or reduce context bloat without using resume/fork.
---

# Fresh Session Handoff

用这个 skill 做两件事：

1. 把当前窗口已经明确的工作状态压缩成一份交接文档。
2. 基于当前 worktree 在 Windows Terminal 中新开一个标签页或窗口，并在 Git Bash 里启动一个 fresh Codex 会话接续工作。

不要把这个 skill 当成 `resume --last` 或 `fork --last` 的替代包装。它的目标是“重建干净上下文”，不是“延续原会话历史”。

## Workflow

### 1. 先确认本次是 fresh session

只有在用户明确要“切新窗口”“开新会话”“刷新上下文”“避免长会话继续膨胀”时使用本 skill。

如果用户要的是：

- 保留完整旧会话上下文
- 继续同一个 session id
- 只是换个终端窗口但不想重建上下文

那不应使用本 skill，应改用 `resume` 或 `fork` 思路。

### 2. 固定生成交接文档

先在当前 worktree 根目录下生成交接文档：

- 目录：`docs/session-handoffs/`
- 文件名：`YYYY-MM-DD-HHMM-<task-slug>-session-handoff.md`

若无法可靠提炼任务简称，用 `general`。

写交接文档时遵守这些约束：

- 只基于当前会话中已经明确出现的信息总结，禁止把推测写成事实。
- 要明确区分：已确认事实、推断、未知。
- 优先保留：当前目标、已完成事项、未完成事项、关键约束、关键文件、建议下一步。
- 内容要短而完整，避免写成长篇复盘。

交接文档结构以 [references/handoff-template.md](./references/handoff-template.md) 为准。

### 3. 生成后立刻自检

生成文档后，至少验证：

- 文件存在
- 文件非空
- 标题和主要章节齐全

若验证失败，先修正文档，再继续。

### 4. 用当前 worktree 拉起 fresh Codex

交接文档生成并验证通过后，运行：

`./scripts/start-fresh-codex-session.ps1 -PromptFile <handoff-file> -WorkingRoot <worktree-root>`

默认行为：

- 如果当前就在 Windows Terminal 中，优先在当前窗口新建标签页
- 如果当前不在 Windows Terminal 中，自动回退为新建窗口
- 新标签页或新窗口的启动目录固定为当前 worktree
- 在 Git Bash 中启动一个 fresh Codex 会话
- 只传入一条简短启动提示，让新会话先读取交接文档

不要把 handoff 全文直接塞给 `codex [PROMPT]`。这样做容易撞上 Windows 命令行长度、转义和特殊字符问题。

如需强制开新窗口，可显式传 `-LaunchMode Window`。

### 5. 汇报方式

完成后只向用户返回最关键的信息：

- handoff 文档保存路径
- 新标签页/窗口是否已成功拉起
- 启动使用的 worktree 根目录
- 如果失败，最小阻塞原因

不要在聊天里重复整份交接文档，除非用户明确要求查看全文。

## Resources

### references/handoff-template.md

交接文档的固定结构和写作边界。

### scripts/start-fresh-codex-session.ps1

基于当前 worktree 在 Windows Terminal 的 Git Bash 中拉起 fresh Codex 会话，默认优先新建标签页。
