# runtime

`modules/workflow/runtime/` 承载 M0 顶层编排运行面的最小 skeleton。

- 允许内容：命令编排、事件编排、归档编排、订阅封装。
- 禁止内容：`ExecutionSession` 状态机、设备轮询、I/O 适配、preflight/first-article/execution concrete。
