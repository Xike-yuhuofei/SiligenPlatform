# tests/performance

正式 performance 验证承载面。

当前 canonical 子域：`tests/performance/`

## Wave 5 仓库级 performance 回链检查点（US6 / M10-M11）

| Checkpoint | 通过标准 | 证据入口 |
|---|---|---|
| `US6-PERF-CP1-surface-anchored` | performance 验证入口稳定承载于 `tests/performance/`，并纳入 `tests/CMakeLists.txt` 的仓库级验证锚点校验 | `tests/performance/README.md`、`tests/CMakeLists.txt` |
| `US6-PERF-CP2-wave5-gates-aligned` | 仓库级性能验证遵循 Wave 5 门禁口径，入口统一通过根级验证链路触发 | `.\\test.ps1`、`python .\\scripts\\migration\\validate_workspace_layout.py`、`python -m test_kit.workspace_validation`、`validation-gates.md` |
| `US6-PERF-CP3-legacy-source-downgraded` | 不再存在平行的 legacy performance 验证根，仓库级 performance owner 已完全收敛到 `tests/performance/` | `tests/performance/README.md`、`wave-mapping.md`、`dsp-e2e-spec-s10-frozen-directory-index.md` |
