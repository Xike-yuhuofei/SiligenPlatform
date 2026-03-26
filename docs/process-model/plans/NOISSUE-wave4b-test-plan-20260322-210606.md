# NOISSUE Wave 4B Test Plan - Current-Fact Scan and Legacy Exit Recheck

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-210606`
- 关联 scope：`docs/process-model/plans/NOISSUE-wave4b-scope-20260322-210606.md`
- 关联 arch：`docs/process-model/plans/NOISSUE-wave4b-arch-plan-20260322-210606.md`

## 1. 验证原则

1. 本阶段不重跑新的全量构建或全量 QA。
2. 验证重点是 current-fact 文本扫描、仓内 legacy-exit 复核和观察期工件完整性。
3. 所有命令在仓库根串行执行，避免复用同一 build root 触发互相踩踏。

## 2. current-fact 文本扫描

```powershell
rg -n "D:\\Projects\\SiligenSuite|config\\machine_config\\.ini|sibling 目录依赖|sibling 运行方式|No commits yet" README.md WORKSPACE.md docs/onboarding docs/runtime docs/troubleshooting integration/hardware-in-loop/README.md
```

预期：

1. 绝对路径 `D:\Projects\SiligenSuite` 不再出现在活跃文档。
2. `config\machine_config.ini` 不再出现在活跃 current-fact 文档。
3. `dxf-pipeline` sibling 依赖不再被写成当前仓内事实。
4. `No commits yet` 不再作为当前 release blocker。

## 3. 仓内门禁复核

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\verify\wave4b-closeout\legacy-exit
```

预期：

1. 报告生成到 `integration\reports\verify\wave4b-closeout\legacy-exit\`
2. 仓内 DXF compat shell / alias 退出规则继续通过
3. 文档 allowlist 仍在可控范围内，没有新的回流

## 4. 工件完整性检查

```powershell
git diff -- README.md WORKSPACE.md docs/onboarding docs/runtime docs/troubleshooting integration/hardware-in-loop/README.md
```

预期：

1. 变更集中在活跃文档和 `Wave 4B` 新增工件。
2. 没有误改产品代码。
3. `docs/runtime/external-migration-observation.md` 与 `wave4b-closeout` 报告目录已落盘。
