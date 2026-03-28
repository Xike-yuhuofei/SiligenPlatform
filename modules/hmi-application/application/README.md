# application

`modules/hmi-application/application/` 现在承载 `M11` 的 Python owner 面。

- `hmi_application.startup`：启动编排、恢复动作、Supervisor 快照收敛。
- `hmi_application.preview_gate`：preview/start gate 与会话级门禁规则。
- `hmi_application.launch_state`：`LaunchResult/SessionSnapshot` 到 UI 状态、恢复按钮使能、online capability 和运行态退化结果的 canonical 收敛。
- `hmi_application.preview_session`：preview 本地状态、payload 规范化、旧契约拒绝、重同步失效与生产前置判定的 canonical owner。
- `hmi_application.supervisor_contract/session`：`M11` 启动状态契约与状态机实现。

`apps/hmi-app` 仅允许消费这些入口，不再在宿主侧保留同源 owner 实现。
`main_window.py` 允许保留的职责固定为 Qt widget/线程装配、消息框展示、status bar 更新、协议调用和 `glue_points + execution_polyline` HTML 渲染。
