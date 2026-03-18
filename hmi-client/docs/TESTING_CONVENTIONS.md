# 测试约定

## 测试分层

- `tests/unit`：ViewModel、协议封装、工具函数。
- `tests/integration/tcp_mock`：HMI 与 mock TCP server 的交互。
- `tests/integration/dxf_editor_mock`：HMI 与 mock DXF editor/notify 文件的交互。

## 最低要求

- 新增界面工作流至少补一个单测或集成测试。
- 协议字段变更必须补 mock 契约回归。
- 不在 HMI 项目中承担真实硬件回归测试。
