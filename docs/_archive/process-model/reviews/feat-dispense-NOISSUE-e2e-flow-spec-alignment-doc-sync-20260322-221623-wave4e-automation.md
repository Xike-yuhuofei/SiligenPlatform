# Doc Sync Review

- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 时间戳：20260322-221623
- 范围：Wave 4E intake/observation automation
- 上游收口：`docs/process-model/plans/NOISSUE-wave4e-scope-20260322-215937.md`

## 1. 本次同步的文件

1. `tools/scripts/register-external-observation-intake.ps1`
2. `tools/scripts/run-external-migration-observation.ps1`
3. `tools/scripts/verify-engineering-data-cli.ps1`
4. `docs/runtime/external-migration-observation.md`
5. `integration/reports/verify/wave4e-rerun/README.md`
6. `integration/reports/verify/wave4e-rerun/intake/README.md`
7. `integration/reports/verify/wave4e-rerun/observation/README.md`

## 2. 关键差异摘要

1. 增加 intake 自动登记脚本，统一输出 `.md` 和 `.json` sidecar。
2. 增加 observation 执行脚本，统一生成 launcher、三类外部输入、`summary.md` 和 `blockers.md`。
3. `verify-engineering-data-cli.ps1` 补充绝对 `ReportDir` 兼容，供 observation 脚本复用。
4. 观察期文档新增推荐执行入口，并明确单文件归档只算 intake，不算可观察目录。
5. `wave4e-rerun` README 系列补齐自动化入口说明，减少后续人工拼装证据的歧义。
