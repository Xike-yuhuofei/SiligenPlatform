# tests/integration/scenarios

这里承载 `L2-offline-integration` 的轻量 smoke 和共享资产复用场景。

## 当前入口

- `run_engineering_regression.py`
- `run_layered_validation_smoke.py`
- `run_shared_asset_reuse_smoke.py`
- `run_hil_controlled_gate_smoke.py`
- `first-layer/`

## 职责

- 校验 root request 如何被路由到 `layer/lane`
- 承载跨 owner 的工程回归 `engineering-regression`
- 校验共享 `samples/`、`tests/baselines/`、`shared/testing/` 没有长出第二事实源
- 校验 controlled HIL gate/release summary 能否在纯离线 synthetic evidence 上完成正式 acceptance 断言
- 承载 HIL 前置所需的离线负例与观测契约
- 为 `quick-gate` / `full-offline-gate` 提供低成本离线 smoke

## 边界

- 这里只做离线 smoke，不承载 simulated-line 或 HIL 真实执行 owner
- 这里只验证共享资产复用和路由语义，不反写 baseline
- `run_hil_controlled_gate_smoke.py` 只构造 synthetic evidence 并调用 gate/release scripts，不替代真实 HIL
- 复杂 acceptance 行为继续留在 `tests/e2e/`
