# Doc Sync Review

- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 时间戳：20260322-210606
- 范围：Wave 4B current-fact 文档归一与仓外观察期文档补齐
- 上游收口：`docs/process-model/plans/NOISSUE-wave4a-closeout-20260322-205132.md`

## 1. 本次同步的文件

1. `README.md`
2. `WORKSPACE.md`
3. `docs/onboarding/developer-workflow.md`
4. `docs/onboarding/workspace-onboarding.md`
5. `docs/runtime/deployment.md`
6. `docs/runtime/release-process.md`
7. `docs/runtime/rollback.md`
8. `docs/troubleshooting/canonical-runbook.md`
9. `integration/hardware-in-loop/README.md`
10. `docs/runtime/external-migration-observation.md`

## 2. 本次同步的原因

1. 活跃文档仍保留绝对路径、旧 build 入口和 root config alias bridge 口径，与当前仓内事实冲突。
2. `Wave 4A` 已结束 DXF compat shell 窗口，但仓外观察期还没有统一执行文档。
3. `release/deployment/rollback/troubleshooting` 需要明确“仓内已验证”和“仓外需人工确认”的边界，避免误宣称迁移完成。

## 3. 关键差异摘要

1. 所有活跃运行文档改为“在仓库根执行”的相对命令，不再硬编码 `D:\Projects\SiligenSuite`。
2. `README.md` 与 `WORKSPACE.md` 不再把 `control-core`、`dxf-pipeline` 写成当前仓内存在路径。
3. `docs/runtime/rollback.md` 删除 `config\machine_config.ini` bridge 恢复步骤，改成唯一 canonical 配置恢复面。
4. `docs/runtime/release-process.md` 不再把 `No commits yet` 当成当前阻塞，改成仓外迁移观察期边界。
5. 新增 `docs/runtime/external-migration-observation.md`，统一外部 launcher、脚本、发布包、回滚包四类人工确认项。

## 4. 关联门禁调整

1. `tools/scripts/legacy_exit_checks.py`
   - 为 `docs/runtime/external-migration-observation.md` 补充活动文档 allowlist
   - 目的不是放宽实现门禁，而是允许观察期文档用负向示例列出已退出的 legacy 入口

## 5. 结论

1. 活跃 current-fact 文档已与当前仓内事实重新对齐。
2. 仓外观察期已有统一入口文档，但仓外迁移是否完成仍需后续人工执行与证据回填。
