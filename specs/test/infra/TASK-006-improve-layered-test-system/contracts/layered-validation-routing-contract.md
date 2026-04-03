# Layered Validation Routing Contract

本合同文件的正式内容已收敛到：

- `shared/testing/test-kit/src/test_kit/validation_layers.py`
- `tests/contracts/test_layered_validation_contract.py`
- `tests/contracts/test_layered_validation_lane_policy_contract.py`

正式路由只允许围绕以下层级展开：

- `L0` 静态与构建门禁
- `L1` 单元测试
- `L2` 模块契约测试
- `L3` 集成测试
- `L4` 模拟全链路 E2E
- `L5` HIL 闭环测试
- `L6` 性能与稳定性测试
