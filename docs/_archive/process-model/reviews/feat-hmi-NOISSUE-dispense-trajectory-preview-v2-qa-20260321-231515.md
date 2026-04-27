# QA Regression Report

1. 环境信息（分支、commit、执行时间）
- 分支：feat/hmi/NOISSUE-dispense-trajectory-preview-v2
- Commit：7a52478e
- 执行时间：2026-03-21 23:15:15 +08:00
- Ticket：NOISSUE

2. 执行命令与退出码
- tools/scripts/resolve-workflow-context.ps1 | ConvertFrom-Json -> 0
- git status --porcelain -> 0
- Get-ChildItem docs/process-model/plans/NOISSUE-test-plan-*.md -> 0

3. 测试范围
- 计划来源：D:\Projects\SiligenSuite\docs\process-model\plans\NOISSUE-test-plan-20260321-221824.md
- 目标范围：HMI 点状轨迹预览、预览确认门禁、dxf.plan.prepare / dxf.preview.snapshot / dxf.preview.confirm / dxf.job.start 真实链路契约一致性
- 实际执行：未启动正式回归（被流程门禁阻断）

4. 失败项与根因
- 失败项：QA 入口门禁失败（工作树非干净）
- 证据：git status --porcelain 返回 79 条改动记录（含已修改与未跟踪文件）
- 根因：违反 ss-qa-regression 规则“非干净工作树不得直接进入 QA”

5. 修复项与验证证据（包含复测命令）
- 本轮未进入“测试失败 -> 修复 -> 复测”闭环，暂无可宣称“已修复”项
- 复测证据：无（流程在 QA 入口即中止）

6. 未修复项与风险接受结论
- 未修复项：完整回归测试未执行
- 风险结论：不接受风险，需先清理工作树并重新执行 QA

7. 发布建议（Go / No-Go）
- 结论：No-Go
- 原因：未满足 QA 入口条件，且缺少可追溯复测证据
