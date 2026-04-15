# Bridge Exit Closeout

更新时间：`2026-04-13`

## 当前冻结口径

- bridge exit 的当前仓库级真值以 `tests/contracts/test_bridge_exit_contract.py`、`scripts/migration/validate_workspace_layout.py`、`scripts/migration/legacy-exit-checks.py` 为准。
- `modules/workflow/application/CMakeLists.txt` 当前只承担 internal-only skeleton 自校验，不再承载 foreign owner wiring。
- canonical 模块骨架目录必须齐全；若某个 owner 面当前没有 source-bearing 内容，也要保留最小占位目录。

## 当前 closeout 条件

- 零 bridge root、零 bridge metadata、零默认 fallback。
- README、测试说明和根级门禁不得再把历史迁移根写成当前默认入口。
- `module-boundary-bridges`、`workspace-layout` 与 `bridge-exit contract` 的规则必须保持同一口径，不能一边要求删除旧 residual，一边又要求恢复旧 wiring。

## 证据回链

- 历史 closeout 记录：`docs/architecture/history/closeouts/bridge-exit-closeout.md`
- 仓库布局 gate：`scripts/migration/validate_workspace_layout.py`
- legacy gate：`scripts/migration/legacy-exit-checks.py`
- boundary gate：`scripts/validation/assert-module-boundary-bridges.ps1`
- contract gate：`tests/contracts/test_bridge_exit_contract.py`
