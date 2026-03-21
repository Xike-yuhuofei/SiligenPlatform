# SiligenSuite Codex 编程工作流

## 1. 目标

在 SiligenSuite 中引入一套可复用、可审计、可落地的 Codex 工程流程，避免“单轮对话式随机产出”。

工作流主链路：

`澄清范围 -> 技术规划 -> 代码审查 -> QA 回归 -> 发布 PR -> 文档同步 -> 周期复盘`

## 2. 命名优化后的技能清单

| 阶段 | 原 gstack 语义 | SiligenSuite 优化名 |
|---|---|---|
| Think | office-hours | `ss-scope-clarify` |
| Plan | plan-eng-review | `ss-arch-plan` |
| Plan(UI) | plan-design-review | `ss-ui-plan-check` |
| Review | review | `ss-premerge-review` |
| Test | qa / qa-only | `ss-qa-regression` |
| Ship | ship | `ss-release-pr` |
| Docs | document-release | `ss-doc-sync` |
| Debug | investigate | `ss-incident-debug` |
| Reflect | retro | `ss-weekly-retro` |
| Safety | careful/freeze/guard | `ss-safety-guard` |
| Router | gstack (总入口) | `ss-workflow` |

命名约束：

- 统一前缀：`ss-`
- 统一结构：`动词-对象`
- 保持阶段可读性：看到名字即可判断用途和输入输出

## 3. 基础执行约束（所有技能共用）

1. 工作目录固定在仓库根：`D:\Projects\SiligenSuite`
2. 分支必须符合格式：`<type>/<scope>/<ticket>-<short-desc>`
3. 失败优先报告事实证据，不允许仅给主观结论
4. 优先复用根级 canonical 命令：
   - `.\build.ps1`
   - `.\test.ps1`
   - `.\ci.ps1`
5. 文档落位固定在 `docs/process-model/` 下，保证团队可见

## 4. 上下文归一化（强制）

所有技能开始前先执行：

```powershell
$CTX = .\tools\scripts\resolve-workflow-context.ps1 | ConvertFrom-Json
```

统一变量：

- `CTX.Branch`：原始分支名
- `CTX.BranchSafe`：文件名安全分支名（`/` 等字符已替换）
- `CTX.Ticket`：从分支提取的任务号；提取失败时固定 `NOISSUE`
- `CTX.Timestamp`：`yyyyMMdd-HHmmss`

禁止在未归一化上下文前直接写入产物文件。

## 5. 标准交付物

| 交付物 | 推荐路径 |
|---|---|
| 范围文档 | `docs/process-model/plans/{ticket}-scope-{timestamp}.md` |
| 架构方案 | `docs/process-model/plans/{ticket}-arch-plan-{timestamp}.md` |
| 测试计划 | `docs/process-model/plans/{ticket}-test-plan-{timestamp}.md` |
| 预审报告 | `docs/process-model/reviews/{branchSafe}-premerge-{timestamp}.md` |
| QA 报告 | `docs/process-model/reviews/{branchSafe}-qa-{timestamp}.md` |
| 发布摘要 | `docs/process-model/reviews/{branchSafe}-release-{timestamp}.md` |
| 故障调查报告 | `docs/process-model/reviews/{ticket}-incident-{timestamp}.md` |
| 周复盘 | `docs/process-model/retro/{yyyy-mm-dd}-{timestamp}.md` |

## 6. 路由优先级（冲突场景）

当用户意图命中多个技能时，优先级如下（高到低）：

1. `ss-safety-guard`（出现高风险命令）
2. `ss-incident-debug`（生产故障/阻断问题）
3. `ss-release-pr`（明确发布请求）
4. `ss-qa-regression`
5. `ss-premerge-review`
6. `ss-arch-plan`
7. `ss-scope-clarify`
8. `ss-doc-sync`
9. `ss-weekly-retro`
10. `ss-ui-plan-check`（作为 UI 变更的并行附加审查）

## 7. 推荐最小闭环

### 7.1 日常特性开发（最常见）

1. `ss-scope-clarify`
2. `ss-arch-plan`
3. 实现代码改动
4. `ss-premerge-review`
5. `ss-qa-regression`
6. `ss-release-pr`
7. `ss-doc-sync`

### 7.2 故障修复（紧急）

1. `ss-incident-debug`
2. 代码修复
3. `ss-premerge-review`
4. `ss-qa-regression`
5. `ss-release-pr`

### 7.3 周度治理

1. `ss-weekly-retro`
2. 根据复盘输出下一周技术债与流程改进行动项
