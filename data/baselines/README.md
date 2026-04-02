# baselines

本目录已降级为 deprecated redirect。

当前迁移口径：

- `data/baselines/simulation/` 已退出，canonical root 为 `tests/baselines/`
- `data/baselines/engineering/` 已退出，canonical root 为 `shared/contracts/engineering/fixtures/cases/rect_diag/`

约束：

- 本目录不得再放入可比对资产文件
- 若重新出现 baseline / fixture 文件，workspace gate 会将其视为 duplicate-source 违规
