# hmi-application

`modules/hmi-application/` 是 `M11 hmi-application` 的 canonical owner 入口。
当前处于 stage-3 residue cleanup：live Python owner package 仍冻结在
`modules/hmi-application/application/hmi_application/`，并已在包内补齐
`hmi_application.contracts.*`、`hmi_application.domain.*`、`hmi_application.services.*`、
`hmi_application.adapters.*` 四类 live 子包；顶层同名目录仍是非 live 占位目录。

## Owner 范围

- HMI 启动恢复、运行态退化、preview 会话与生产前置校验的 owner 语义。
- 人机展示对象、状态聚合与 host-to-owner 边界。
- HMI 域 owner 专属 contracts 与宿主调用边界。

## Owner 入口

- 构建入口：`modules/hmi-application/CMakeLists.txt`（target：`siligen_module_hmi_application`）。
- live Python owner 入口：`modules/hmi-application/application/hmi_application/`
- 台账入口：`docs/architecture-audits/modules/hmi-application/residue-ledger.md`
- 测试入口：`modules/hmi-application/tests/CMakeLists.txt`。

## 边界约束

- `apps/hmi-app/`、`apps/control-cli/` 仅承担宿主/命令入口，不承载 `M11` 终态 owner 事实。
- `apps/hmi-app/src/hmi_client/ui/main_window.py` 只允许保留 Qt 宿主、signal/slot、消息框、status bar 更新、协议调用和 HTML 渲染。
- 启动恢复、preview 语义校验、preview 重同步与生产前置 block reason 必须落在 `modules/hmi-application/application/hmi_application/`。
- `hmi_application.adapters.*` 只承载包内 runtime-concrete 支撑，不得反向长出新的 façade / compat root。
- `M11` 仅承载 HMI 业务 owner 语义，不直接越权承担设备实现或运行时执行 owner 职责。

## 当前 live owner surface

- `hmi_application.__init__` 不再承担 public surface 聚合；consumer 必须显式从下列子模块导入稳定 seam。
- `modules/hmi-application/application/hmi_application/launch_supervision_session.py`
- `modules/hmi-application/application/hmi_application/launch_state.py`
- `modules/hmi-application/application/hmi_application/preview_session.py`
- `modules/hmi-application/application/hmi_application/domain/preview_session_types.py`
- `modules/hmi-application/application/hmi_application/domain/launch_result_types.py`
- `modules/hmi-application/application/hmi_application/services/preview_playback.py`
- `modules/hmi-application/application/hmi_application/services/preview_payload_authority.py`
- `modules/hmi-application/application/hmi_application/services/preview_preflight.py`
- `modules/hmi-application/application/hmi_application/services/preview_status_projection.py`
- `modules/hmi-application/application/hmi_application/services/startup_orchestration.py`
- `modules/hmi-application/application/hmi_application/startup.py`
- `modules/hmi-application/application/hmi_application/contracts/launch_supervision_contract.py`
- `modules/hmi-application/application/hmi_application/domain/launch_supervision_types.py`
- `modules/hmi-application/application/hmi_application/services/launch_supervision_flow.py`
- `modules/hmi-application/application/hmi_application/services/launch_supervision_runtime.py`
- `modules/hmi-application/application/hmi_application/adapters/launch_supervision_env.py`
- `modules/hmi-application/application/hmi_application/adapters/launch_supervision_ports.py`

## 当前 live package split

- `hmi_application.launch_supervision_session`
  - 稳定 session seam；只保留 `SupervisorSession` / `SupervisorPolicy` 入口
- `hmi_application.contracts.launch_supervision_contract`
  - 当前 live 启动监督态 contract
- `hmi_application.domain.launch_supervision_types`
  - 启动监督态值对象、超时策略、stage 常量与 mode 规范化
- `hmi_application.domain.launch_result_types`
  - `LaunchResult` / `StartupResult` 与启动快照投影
- `hmi_application.services.launch_supervision_flow`
  - 启动/恢复状态机流程与事件/快照发射
- `hmi_application.services.launch_supervision_runtime`
  - 运行态退化识别与 failure snapshot 构造
- `hmi_application.services.startup_orchestration`
  - startup 薄 orchestration seam；日志与 startup 子 service 收敛
- `hmi_application.services.launch_result_projection`
  - `LaunchResult`、offline result 与 snapshot projection
- `hmi_application.services.startup_sequence_service`
  - startup/recovery sequence 装配与 `SupervisorSession` 委托
- `hmi_application.adapters.launch_supervision_env`
  - 启动超时 env 解析与 `SupervisorPolicy` 装配
- `hmi_application.adapters.launch_supervision_ports`
  - backend / TCP / hardware 端口协议
- `hmi_application.adapters.startup_qt_workers`
  - `StartupWorker` / `RecoveryWorker` 的包内 Qt worker 实现
- `hmi_application.launch_state`
  - launch UI state、恢复动作判定、运行态退化/恢复投影
- `hmi_application.preview_session`
  - 稳定 preview session seam；保留 `PreviewSessionOwner` 与 preview 类型 re-export
- `hmi_application.domain.preview_session_types`
  - preview 状态、preflight decision、payload/result DTO
- `hmi_application.services.preview_playback`
  - 本地 motion preview 播放控制与状态推进
- `hmi_application.services.preview_payload_authority`
  - payload authority 校验、resync、plan invalidation 与错误归一
- `hmi_application.services.preview_preflight`
  - confirmation / preflight decision / diagnostic notice
- `hmi_application.services.preview_status_projection`
  - preview 状态、来源、轨迹摘要与 info label 投影
- `hmi_application.startup`
  - 稳定 startup seam；保留 `LaunchResult`、policy loader 与 launch/recovery 入口，不再转发 worker
- `hmi_application.adapters.qt_workers`
  - `PreviewSnapshotWorker` 与 startup worker re-export；worker surface 不再经 `preview_session` / `startup` 转发
- `hmi_application.adapters.logging_support`
  - 模块内文件日志 sink

## 顶层目录状态

- 当前各个顶层占位目录仍非 live implementation roots，不在 `SILIGEN_TARGET_IMPLEMENTATION_ROOTS`。
- 物理抽离到这些顶层目录的批次尚未执行完成；在此之前，不得把这些占位目录继续写成已 closeout。
- 模块内 `supervisor_*` compat shim 仍在退场过程中；当前不属于 canonical surface，后续批次再清退。

## 当前测试面

- `tests/unit/`
  - 冻结 launch state projection、startup-state 与 preview protocol consumption
- `tests/contract/`
  - 冻结 `launch_supervision_contract.py` 的快照/事件不变量
- `tests/bootstrap.py`
  - 统一 owner test bootstrap；模块测试不再各自内嵌 application path 注入
