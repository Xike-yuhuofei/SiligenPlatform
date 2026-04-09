# tests

hmi-application 的模块级验证入口应收敛到此目录。

当前最小正式测试面：

- `bootstrap.py`
  - canonical test bootstrap；统一 application path 注入
- `unit/`
  - `test_launch_state.py`：状态投影与 startup-state
  - `test_launch_result_projection.py`：`LaunchResult` / offline result / snapshot projection
  - `test_preview_session.py`：`PreviewSessionOwner` 稳定 seam 与 owner 级副作用
  - `test_preview_payload_authority.py`：preview payload authority、resync 与失效收敛
  - `test_preview_preflight.py`：confirmation / preflight decision / diagnostic notice
  - `test_preview_playback.py`：本地 motion preview 播放状态机
  - `test_preview_snapshot_worker.py`：preview worker 超时预算与完成信号
  - `test_startup_orchestration.py`：startup/recovery orchestration seam
  - `test_startup_qt_workers.py`：startup worker split 后的 Qt signal/委托与 re-export
- `contract/`
  - `test_launch_supervision_contract.py`：`hmi_application.contracts.launch_supervision_contract` 的 `SessionSnapshot` / `SessionStageEvent` 不变量

当前未补：

- presenter/view-model 更细粒度映射样本
- host-to-owner 邻接集成回归
- HMI 启动链的更完整 fault/recovery 矩阵

当前不再伪装为正式 suite：

- 两个额外占位测试分层目录

它们当前仅作为 workspace layout-required skeleton 保留，尚未补齐真实测试，因此不进入当前 canonical runner。
