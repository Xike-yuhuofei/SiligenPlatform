# NOISSUE Wave 4B Closeout

- 状态：Done
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-210606`
- 上游收口：`docs/process-model/plans/NOISSUE-wave4a-closeout-20260322-205132.md`
- 关联计划：
  - `docs/process-model/plans/NOISSUE-wave4b-scope-20260322-210606.md`
  - `docs/process-model/plans/NOISSUE-wave4b-arch-plan-20260322-210606.md`
  - `docs/process-model/plans/NOISSUE-wave4b-test-plan-20260322-210606.md`
- 关联验证目录：`integration/reports/verify/wave4b-closeout/`

## 1. 总体裁决

1. `Wave 4B repo-side preparation = Done`
2. `external observation execution = Go`
3. `external migration complete declaration = No-Go`
4. `next-wave planning = Conditional Go`

## 2. 本阶段实际完成内容

### 2.1 活跃 current-fact 文档收口

1. `README.md`、`WORKSPACE.md`、`docs/onboarding/*`、`docs/runtime/*`、`docs/troubleshooting/canonical-runbook.md`、`integration/hardware-in-loop/README.md`
   - 删除活跃文档中的绝对路径 `D:\Projects\SiligenSuite`
   - 删除把 `dxf-pipeline` sibling 依赖写成当前仓内事实的表述
   - 删除把 `config\machine_config.ini` 写成当前 bridge 的表述
   - 明确所有命令默认在仓库根执行

### 2.2 发布 / 部署 / 回滚 / 排障口径归一

1. `docs/runtime/deployment.md`
   - 明确本文档只覆盖仓内 canonical 交付口径
2. `docs/runtime/release-process.md`
   - 把阻塞项改成“仓外迁移仍需人工确认”的真实边界
3. `docs/runtime/rollback.md`
   - 回滚只恢复 `config\machine\machine_config.ini`
   - `config\machine_config.ini` 只作为已删除 alias 的负向说明存在
4. `docs/troubleshooting/canonical-runbook.md`
   - 把 root config alias 改成“误按旧文档恢复已删除 alias”的失败模式

### 2.3 仓外观察期入口与门禁同步

1. 新增 `docs/runtime/external-migration-observation.md`
   - 固定外部 launcher、已部署脚本、发布包、回滚包四类检查项
   - 固定 `Go/No-Go` 规则和命中 legacy 入口时的处理动作
2. `tools/scripts/legacy_exit_checks.py`
   - 为观察期文档补充活动文档 allowlist
   - 保证负向示例不会被误判成仓内回流

## 3. 实际执行命令

1. `rg -n "D:\\Projects\\SiligenSuite|sibling 目录依赖|sibling 运行方式|No commits yet" README.md WORKSPACE.md docs/onboarding docs/runtime docs/troubleshooting integration/hardware-in-loop/README.md`
2. `rg -n "config\\machine_config\\.ini" README.md WORKSPACE.md docs/onboarding docs/runtime docs/troubleshooting integration/hardware-in-loop/README.md`
3. `powershell -NoProfile -ExecutionPolicy Bypass -File .\legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\verify\wave4b-closeout\legacy-exit`

## 4. 关键证据

1. `integration/reports/verify/wave4b-closeout/docs/current-fact-scan.md`
   - 绝对路径 / sibling 依赖 / `No commits yet` 零命中
   - root config alias 只剩负向说明
2. `integration/reports/verify/wave4b-closeout/legacy-exit/legacy-exit-checks.md`
   - `passed_rules=19 failed_rules=0`
   - `active-docs-legacy-entry-allowlist=PASS`
3. `integration/reports/verify/wave4b-closeout/observation/observation-readiness.md`
   - `repo-side observation preparation = Done`
   - `external observation execution = Pending`

## 5. 阶段结论

1. `Wave 4B` 已完成仓内侧的文档 current-fact 收口与观察期准备。
2. 当前仓库已经提供统一的仓外迁移观察期入口，但这不等于仓外环境已完成迁移。
3. 后续若继续推进，优先顺序应为：
   - 先执行 `external-migration-observation.md` 的人工确认并回填证据
   - 再根据观察期结论决定是否进入下一波次规划

## 6. 非阻断记录

1. 当前工作树仍明显不干净，本阶段没有回退无关改动。
2. 本阶段未重跑新的全量 build/test；这是文档阶段刻意控制的验证范围，不影响本次结论。
3. 若仓外交付物命中 legacy DXF 入口或 root config alias，处理动作只能是替换仓外交付物并重验，不能恢复 compat shell。
