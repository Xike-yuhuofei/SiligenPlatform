# 测试约定

## 测试分层

- `tests/unit`：几何算法、路径处理、轨迹采样、报告逻辑。
- `tests/integration/proto-compat`：proto 契约兼容性。
- `tests/integration/trajectory-generation`：端到端生成流程。
- `tests/integration/control-core-fixtures`：与控制核心样例的回归验证。

## 最低要求

- proto 变更必须补兼容性测试。
- 输出 JSON/PB 字段变更必须补回归样例。
- 性能相关改动建议更新 benchmark 样例记录。
