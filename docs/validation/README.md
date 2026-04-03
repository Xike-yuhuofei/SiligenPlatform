# Validation Docs

验证文档当前正式口径：

- 根级入口见 `docs/architecture/build-and-test.md`
- 分层体系见 `docs/architecture/layered-test-system-v2.md`
- 覆盖矩阵见 `docs/architecture/test-coverage-matrix-v1.md`
- 机台前阻断见 `docs/architecture/machine-preflight-quality-gate-v1.md`

全仓只允许以下正式层级：

- `L0` 静态与构建门禁
- `L1` 单元测试
- `L2` 模块契约测试
- `L3` 集成测试
- `L4` 模拟全链路 E2E
- `L5` HIL 闭环测试
- `L6` 性能与稳定性测试
