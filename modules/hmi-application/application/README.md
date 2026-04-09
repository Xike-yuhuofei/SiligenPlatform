# application

`modules/hmi-application/application/` 现在承载 `M11` 的 Python owner 面。
`hmi_application.__init__` 现在只保留包声明，不再做默认 re-export；consumer 必须显式导入子模块 seam。

- `hmi_application.contracts.launch_supervision_contract`：`M11` 启动监督态稳定 contract。
- `hmi_application.domain.launch_supervision_types`：启动监督态值对象、超时策略、stage 常量与 mode 规范化。
- `hmi_application.domain.launch_result_types`：`LaunchResult/StartupResult` 与启动快照投影。
- `hmi_application.services.launch_supervision_flow`：启动/恢复状态机流程与事件/快照发射。
- `hmi_application.services.launch_supervision_runtime`：运行态退化与 failure snapshot helper。
- `hmi_application.services.launch_result_projection`：`LaunchResult` / offline result / snapshot projection。
- `hmi_application.services.startup_sequence_service`：startup/recovery sequence 装配与 `SupervisorSession` 委托。
- `hmi_application.services.startup_orchestration`：startup 薄 orchestration seam 与结果日志收敛。
- `hmi_application.domain.preview_session_types`：preview 状态、preflight decision、payload/result DTO。
- `hmi_application.services.preview_playback`：本地 motion preview 播放控制。
- `hmi_application.services.preview_payload_authority`：preview payload authority 校验、resync、plan invalidation 与错误归一。
- `hmi_application.services.preview_preflight`：confirmation / preflight decision / diagnostic notice。
- `hmi_application.services.preview_status_projection`：preview 状态、来源、轨迹摘要与 info label 投影。
- `hmi_application.launch_supervision_session`：稳定 session seam；外部继续从这里拿 `SupervisorSession/SupervisorPolicy`。
- `hmi_application.startup`：稳定 startup seam；继续提供 `LaunchResult`、policy loader 与 launch/recovery 入口。
- `hmi_application.preview_gate`：preview/start gate 与会话级门禁规则。
- `hmi_application.launch_state`：`LaunchResult/SessionSnapshot` 到 UI 状态、恢复按钮使能、online capability 和运行态退化结果的 canonical 收敛。
- `hmi_application.preview_session`：稳定 preview owner seam；继续提供 `PreviewSessionOwner` 与 preview 类型入口。
- `hmi_application.adapters.launch_supervision_env`：启动超时 env 解析与 `SupervisorPolicy` 装配。
- `hmi_application.adapters.qt_workers`：preview worker 入口与 startup worker re-export；worker 不再在 owner 根文件里直接实现，也不再经 `startup` 转发。
- `hmi_application.adapters.startup_qt_workers`：`StartupWorker` / `RecoveryWorker` 的包内 Qt worker 实现。
- `hmi_application.adapters.launch_supervision_ports`：backend / TCP / hardware port protocol。
- `hmi_application.adapters.logging_support`：模块内文件日志 sink。

`apps/hmi-app` 仅允许消费这些入口，不再在宿主侧保留同源 owner 实现。
`main_window.py` 允许保留的职责固定为 Qt widget/线程装配、消息框展示、status bar 更新、协议调用和 `glue_points + motion_preview` HTML 渲染。
模块内 `supervisor_*` compat shim 仍处于退场阶段，但不再作为 canonical package surface。
