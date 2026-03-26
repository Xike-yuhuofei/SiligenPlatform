# QA Report

## 1. 环境信息

1. 分支：`feat/dispense/NOISSUE-e2e-flow-spec-alignment`
2. 基线提交：`f6e5668d`
3. 执行时间：`2026-03-22`
4. 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-210606`
5. 说明：本轮只验证 `Wave 4B` 的文档 current-fact 收口和 `legacy-exit` 复核，不重跑全量 build/test

## 2. 执行命令与退出码

1. `rg -n "D:\\Projects\\SiligenSuite|sibling 目录依赖|sibling 运行方式|No commits yet" README.md WORKSPACE.md docs/onboarding docs/runtime docs/troubleshooting integration/hardware-in-loop/README.md`
   - exit code：`1`
   - 解释：无匹配，按零命中通过
2. `rg -n "config\\machine_config\\.ini" README.md WORKSPACE.md docs/onboarding docs/runtime docs/troubleshooting integration/hardware-in-loop/README.md`
   - exit code：`0`
   - 解释：仅命中负向说明，不代表 alias 回流
3. `powershell -NoProfile -ExecutionPolicy Bypass -File .\legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\verify\wave4b-closeout\legacy-exit`
   - 首次 exit code：`1`
   - 复测 exit code：`0`

## 3. 首次失败项与修复

1. 首次 `legacy-exit-check.ps1` 失败
   - 根因：新增 `docs/runtime/external-migration-observation.md` 使用 legacy 命令名做负向排查示例，但未登记到 `active-docs-legacy-entry-allowlist`
2. 修复：
   - `tools/scripts/legacy_exit_checks.py` 增加 `docs/runtime/external-migration-observation.md` allowlist
3. 复测结果：
   - `passed_rules=19 failed_rules=0`

## 4. 当前结论

1. 活跃文档中的绝对路径、sibling 依赖和旧 release blocker 已完成清理。
2. `config\machine_config.ini` 仅作为“已删除 alias / 禁止恢复项”出现在负向说明中，没有回到 canonical 当前入口。
3. `legacy-exit` 仓内门禁继续通过，没有因为本轮文档调整而破坏退出状态。

## 5. 未覆盖项

1. 未执行新的全量 build/test；这是文档阶段的刻意选择，避免在脏工作树和同一 build root 上重复触发大回归。
2. 未执行仓外真实观察期；当前仍不能宣称外部迁移完成。
