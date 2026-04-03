# Layered Test Matrix

此文件保留为验证目录的入口索引，正式 authority 已切到：

- `docs/architecture/layered-test-system-v2.md`
- `docs/architecture/test-coverage-matrix-v1.md`
- `docs/architecture/machine-preflight-quality-gate-v1.md`

## 当前正式分层

| 层级 | 含义 | 主承载面 |
|---|---|---|
| `L0` | 静态与构建门禁 | 根级入口、`tests/reports/static/` |
| `L1` | 单元测试 | `apps/*/tests/unit`、`modules/*/tests/unit` |
| `L2` | 模块契约测试 | `tests/contracts/`、`tests/integration/protocol-compatibility/` |
| `L3` | 集成测试 | `tests/integration/scenarios/` |
| `L4` | 模拟全链路 E2E | `tests/e2e/simulated-line/` |
| `L5` | HIL 闭环测试 | `tests/e2e/hardware-in-loop/` |
| `L6` | 性能与稳定性测试 | `tests/performance/` |

## 当前 suite taxonomy

| suite | 主层级 | 标签 |
|---|---|---|
| `apps` | `L0`,`L1` | `suite:apps` |
| `contracts` | `L2` | `suite:contracts` |
| `protocol-compatibility` | `L2` | `suite:protocol-compatibility` |
| `integration` | `L3` | `suite:integration` |
| `e2e` | `L4` | `suite:e2e` |
| `performance` | `L6` | `suite:performance` |
