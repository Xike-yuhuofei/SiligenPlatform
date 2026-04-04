# scripts

`scripts/` 是仓库级自动化与迁移脚本的正式承载根（canonical root）。

## 正式职责

- 承载根级自动化实现（构建、测试、校验、迁移辅助）。
- 承载 `scripts/validation/` 与 `scripts/migration/` 的正式脚本资产。
- 作为工作区唯一脚本根，对外提供稳定入口。

## 非职责边界

- 不承载业务模块 owner 实现。
- 不承载运行时临时产物、报告输出或缓存文件。
- 不通过历史目录建立并行脚本入口。

## 回流约束

- 已删除旧脚本根；不得通过恢复历史目录绕过 `scripts/`。
- 新增或重构自动化逻辑时，默认直接写入 `scripts/`。
- 若涉及门禁或冻结文档链路，需同步更新治理文档与验证入口。

## Codex 启动工作流

- 正式入口：`scripts/validation/start-codex-club-from-file.ps1`
- 作用：读取指定 Prompt 文档全文，使用 Windows Terminal 默认启动目录打开 Git Bash，并执行 `codex --yolo --profile <profile> "<prompt全文>"`。
- 特性：
  - 固定使用 `wt -w 0 new-tab`，避免一次触发多个窗口。
  - 自动记录日志到 `logs/codex-launch/`。
  - 支持读取用户级配置 `~/.codex/notify.local.json`，在任务结束后通过“虾推啥”发送微信通知。
  - 默认保留 shell，便于失败时直接查看终端输出。

示例：

```powershell
.\scripts\validation\start-codex-club-from-file.ps1 `
  -PromptFile "<ABS_PROMPT_FILE>" `
  -Profile "club"
```
