---
name: ss-workflow
description: |
  SiligenSuite 工作流路由技能。根据用户意图选择下一阶段技能，并统一执行
  分支命名、证据输出、根级命令和交付物落盘规范。
---

# /ss-workflow

## 步骤 0：上下文归一化（必须先做）

```powershell
$CTX = .\tools\scripts\resolve-workflow-context.ps1 | ConvertFrom-Json
```

如果 `CTX.BranchCompliant = false`，先提示分支不合规并要求改名，再继续路由。

## 作用

当用户只给出目标、尚未明确当前阶段时，先确定流程位置，再切换到对应技能。

## 冲突优先级（高到低）

1. `ss-safety-guard`
2. `ss-incident-debug`
3. `ss-release-pr`
4. `ss-qa-regression`
5. `ss-premerge-review`
6. `ss-arch-plan`
7. `ss-scope-clarify`
8. `ss-doc-sync`
9. `ss-weekly-retro`
10. `ss-ui-plan-check`（UI 变更场景下的附加审查）

## 路由规则

1. 需求不清晰、边界模糊：`ss-scope-clarify`
2. 已有需求，需技术方案与测试策略：`ss-arch-plan`
3. 代码已改完，准备预审：`ss-premerge-review`
4. 需要测试/修复/回归：`ss-qa-regression`
5. 准备提交 PR：`ss-release-pr`
6. 需要同步 README/架构文档：`ss-doc-sync`
7. 出现线上或联调故障：`ss-incident-debug`
8. 周期性复盘：`ss-weekly-retro`
9. 涉及高风险命令：先启用 `ss-safety-guard`
10. 需求涉及 HMI 交互或视觉规范：追加 `ss-ui-plan-check`

## 全局硬约束

1. 工作目录必须为仓库根：`D:\Projects\SiligenSuite`
2. 未做上下文归一化前禁止生成任何产物文件
3. 未达证据门槛时，禁止给“可发布”结论
4. 默认优先根级命令：`.\build.ps1`、`.\test.ps1`、`.\ci.ps1`
