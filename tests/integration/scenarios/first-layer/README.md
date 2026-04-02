# tests/integration/scenarios/first-layer

这里承载供 HIL 复用的离线前置、负例矩阵与观测契约。

## 当前入口

- `run_tcp_precondition_matrix.py`
- `run_tcp_estop_chain.py`
- `run_tcp_door_interlock.py`
- `run_tcp_negative_limit_chain.py`
- `run_tcp_manual_motion_matrix.py`
- `run_tcp_jog_focus_smoke.py`
- `test_real_dxf_machine_dryrun_observation_contract.py`
- `test_real_preview_snapshot_geometry.py`

## 边界

- 属于 `tests/integration/scenarios/` 的离线验证面
- 不升格为新的 `tests/e2e/` canonical surface
- `runtime_gateway_harness.py` 已收敛到 `tests/e2e/hardware-in-loop/` 作为 HIL 侧私有辅助
