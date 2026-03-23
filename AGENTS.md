# SiligenSuite Agent Rules

## 回应原则（最高优先级）
- 必须保持严格中立、专业和工程导向的回应风格。
- 判断只能基于事实、逻辑和技术依据，不得因用户语气而迎合、附和或改变结论。
- 禁止使用模棱两可的表述，结论必须明确且可执行。

## 语言
- 必须使用简体中文与用户沟通。

## Git 分支命名（强制）
- 所有新建分支必须使用统一格式：`<type>/<scope>/<ticket>-<short-desc>`。
- 不符合该格式的分支禁止继续开发，必须先重命名为合规名称。

### 字段约束
- `type`：`feat`、`fix`、`chore`、`refactor`、`docs`、`test`、`hotfix`、`spike`、`release`。
- `scope`：受影响模块或域名，推荐短名，如 `hmi`、`runtime`、`cli`、`gateway`。
- `ticket`：任务系统编号（如 `SS-142`）。若确实没有任务号，使用 `NOISSUE` 占位并尽快补齐。
- `short-desc`：英文小写短描述，使用 kebab-case，例如 `debug-instrumentation`。

### 示例
- `chore/hmi/SS-142-debug-instrumentation`
- `fix/runtime/SS-205-startup-timeout`
- `chore/hmi/NOISSUE-debug-instrumentation`

## Codex 技能工作流（项目内）

项目内技能入口：`.agents/skills/`

推荐按以下顺序使用：

1. `ss-scope-clarify`：需求澄清与范围锁定
2. `ss-arch-plan`：架构与测试计划
3. `ss-premerge-review`：预合并代码审查
4. `ss-qa-regression`：测试-修复-回归
5. `ss-release-pr`：发版与 PR 提交
6. `ss-doc-sync`：发布后文档同步
7. `ss-weekly-retro`：阶段性复盘

按需技能：

- `ss-ui-plan-check`：UI/交互方案审查（HMI 相关需求）
- `ss-incident-debug`：故障调查与根因分析
- `ss-safety-guard`：高风险操作安全护栏

命名优化原则：

- 统一前缀 `ss-`（SiligenSuite）
- 统一“动词-对象”结构，减少语义歧义
- 名称直接对应交付阶段，便于团队记忆与检索

执行约束补充：

1. 运行技能前，先执行 `tools/scripts/resolve-workflow-context.ps1` 统一 `ticket/branchSafe/timestamp`
2. 高风险命令必须通过 `tools/scripts/invoke-guarded-command.ps1` 执行
