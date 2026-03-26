# tests/integration

正式 integration 验证承载面。

当前 canonical 子域：`tests/integration/scenarios/`、`tests/integration/protocol-compatibility/`

## Wave 2 上游链回链检查点（US3 / M1-M3）

| Checkpoint | 通过标准 | 证据入口 |
|---|---|---|
| `US3-CP1-owner-paths` | `modules/job-ingest/`、`modules/dxf-geometry/`、`modules/topology-feature/` 均作为正式 owner 路径存在，并具备对应入口说明 | `modules/job-ingest/`、`modules/dxf-geometry/`、`modules/topology-feature/` |
| `US3-CP2-samples-wired` | 上游链样本入口与 owner 对齐，`samples/dxf/` 和 `samples/golden/` 作为静态工程事实样本来源 | `samples/dxf/`、`samples/golden/`、`wave-mapping.md` |
| `US3-CP3-legacy-downgraded` | 上游链 legacy 来源不再承担 `M1-M3` 终态 owner；所有 live owner 面均已切到 canonical roots | `apps/planner-cli/`、`modules/dxf-geometry/application/engineering_data/`、`module-cutover-checklist.md` |
| `US3-CP4-validation-gates` | `Wave 2` 门禁命令与回链校验口径一致，可用于上游链收敛验收 | `python .\\scripts\\migration\\validate_workspace_layout.py --wave \"Bridge Exit Closeout\"`、`.\\test.ps1`、`validation-gates.md` |

## Wave 5 仓库级验证承载检查点（US6 / M10-M11）

| Checkpoint | 通过标准 | 证据入口 |
|---|---|---|
| `US6-CP1-tests-root-canonical` | `tests/` 成为仓库级验证唯一承载根，`tests/CMakeLists.txt` 显式校验三个仓库级验证 README 锚点存在 | `tests/CMakeLists.txt`、`tests/integration/README.md`、`tests/e2e/README.md`、`tests/performance/README.md` |
| `US6-CP2-trace-hmi-owner-wired` | `M10`/`M11` owner 已归位到 `modules/trace-diagnostics/` 与 `modules/hmi-application/`，仓库级验证入口只做验证承载，不回写业务 owner | `modules/trace-diagnostics/README.md`、`modules/hmi-application/README.md`、`module-cutover-checklist.md` |
| `US6-CP3-wave5-gates-aligned` | `Wave 5` 门禁口径与根级验证入口一致，可直接用于收敛验收 | `.\\test.ps1`、`python .\\scripts\\migration\\validate_workspace_layout.py`、`python -m test_kit.workspace_validation`、`docs/validation/README.md`、`validation-gates.md` |
| `US6-CP4-legacy-integration-downgraded` | 不再存在平行的 legacy integration 验证根，仓库级验证 owner 已完全收敛到 `tests/` | `tests/integration/README.md`、`wave-mapping.md`、`dsp-e2e-spec-s10-frozen-directory-index.md` |
