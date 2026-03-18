# 测试约定

## 测试分层

- `tests/unit`：覆盖 domain、application、纯算法与纯规则。
- `tests/integration`：覆盖 TCP、运行时流程、Mock 硬件、配方执行。
- `tests/contract`：覆盖与 HMI、DXF pipeline 的输入输出契约。

## 最低要求

- 新增用例必须至少有一个单测或契约测试。
- 涉及协议字段变更，必须补对应 contract test。
- 涉及硬件控制路径变更，至少补 Mock 硬件回归测试。

## 命名建议

- 单测：`<Module>NameTest.cpp`
- 契约测试：`<Contract>NameContractTest.cpp`
- 集成测试：`<Flow>NameIntegrationTest.cpp`
