# tests/integration

正式 integration 验证承载面。

## 正式层级

- `tests/integration/protocol-compatibility/`：`L2` 模块契约测试
- `tests/integration/scenarios/`：`L3` 集成测试

## 当前 authority path

- `tests/integration/protocol-compatibility/run_protocol_compatibility.py`
- `tests/integration/scenarios/run_engineering_regression.py`
- `tests/integration/scenarios/run_preview_flow_regression.py`
- `apps/hmi-app/scripts/online-smoke.ps1`

## 说明

- `L3` 是机台前主发现层，不依赖真实机台也要能发现主链问题。
- `tests/integration/scenarios/first-layer/` 保留为联机前观测与负例资产，不单独升格为 `L4/L5`。
- integration 报告必须带执行命令、样本标识、通过/失败结果、失败摘要和报告路径。
