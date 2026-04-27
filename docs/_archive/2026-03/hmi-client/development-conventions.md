# 开发规范

## UI 分层

- `ui` 负责展示与事件绑定。
- `viewmodels` 负责状态组织与命令编排。
- `client` 负责与 `control-core` 的通信。
- `integrations` 负责与外部子项目的契约适配。

## 编码原则

- 禁止在 UI 组件中直接操作 socket、文件协议或外部进程细节。
- 用户可见文案、错误提示、状态标签应集中管理。
- 新界面功能优先配套一个 workflow 或 viewmodel，而不是把逻辑写进窗口类。

## 协作原则

- 与主控协作只走 TCP/JSON 契约。
- 与 DXF editor 协作只走启动参数和 notify 契约。
- 协议变更必须先改文档，再改实现。
