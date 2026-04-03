# TASK-006 Quickstart

本任务的正式分层、gate 与覆盖矩阵已冻结到：

- `docs/architecture/layered-test-system-v2.md`
- `docs/architecture/test-coverage-matrix-v1.md`
- `docs/architecture/machine-preflight-quality-gate-v1.md`

当前全仓只允许以下层级：

- `L0` 静态与构建门禁
- `L1` 单元测试
- `L2` 模块契约测试
- `L3` 集成测试
- `L4` 模拟全链路 E2E
- `L5` HIL 闭环测试
- `L6` 性能与稳定性测试
