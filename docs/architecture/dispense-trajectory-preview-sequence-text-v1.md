# 点胶轨迹预览按函数调用顺序时序图文本版 V1

更新时间：`2026-03-26`

## 1. 目标与适用范围

本文档以“按函数调用顺序展开”的方式，固化 HMI 轨迹预览链路的文本版时序图，用于代码走查、联调定位和文档引用。

适用范围：

1. `apps/hmi-app` 中轨迹预览相关 HMI 入口、状态门禁与启动前校验。
2. `apps/runtime-gateway/transport-gateway` 中 `dxf.plan.prepare`、`dxf.preview.snapshot`、`dxf.preview.confirm`、`dxf.job.start` 的 transport 映射。
3. `modules/workflow` 中点胶预览快照、确认态与启动门禁的 runtime 逻辑。

不包含：

1. 真实机台硬件行为验收。
2. 图形版 Mermaid 时序图。
3. 预览算法之外的工艺效果承诺。

## 2. 参与者

参与者：

- 用户
- `MainWindow(HMI)`
- `PreviewSnapshotWorker`
- `CommandProtocol`
- `TcpClient`
- `TcpCommandDispatcher(gateway)`
- `DispensingWorkflowUseCase(runtime)`
- `DispensePreviewGate(HMI 本地门禁)`

## 3. 按函数调用顺序展开的时序图文本版

```text
正常预览链：

1. 用户选择 DXF
   用户 -> MainWindow._on_dxf_load()
   MainWindow -> CommandProtocol.dxf_create_artifact()
   CommandProtocol -> TcpClient.send_request("dxf.artifact.create")
   TcpClient -> gateway
   gateway -> runtime.CreateArtifact(...)
   runtime -> gateway: artifact_id / segment_count
   gateway -> TcpClient -> CommandProtocol -> MainWindow
   MainWindow: 保存 artifact_id，reset gate 为 DIRTY，随后自动调用 _generate_dxf_preview()

2. HMI 请求生成预览
   MainWindow._generate_dxf_preview()
   MainWindow -> DispensePreviewGate.begin_preview()  状态: GENERATING
   MainWindow -> PreviewSnapshotWorker.start()

3. Worker 先准备 plan
   PreviewSnapshotWorker.run()
   Worker -> CommandProtocol.dxf_prepare_plan(artifact_id, speed, dry_run)
   CommandProtocol -> TcpClient.send_request("dxf.plan.prepare")
   TcpClient -> gateway.HandleDxfPlanPrepare()
   gateway -> runtime.PreparePlan()
   runtime: 生成 plan_id / plan_fingerprint / trajectory_points / 统计信息
   runtime -> gateway -> TcpClient -> Worker

4. Worker 再拉取 snapshot
   Worker -> CommandProtocol.dxf_preview_snapshot(plan_id, max_polyline_points=4000)
   CommandProtocol -> TcpClient.send_request("dxf.preview.snapshot")
   TcpClient -> gateway.HandleDxfPreviewSnapshot()
   gateway -> runtime.GetPreviewSnapshot(plan_id, max_polyline_points)
   runtime: trajectory_points
         -> 去重
         -> 去短 ABA 回退
         -> 3mm 点距采样
         -> 保角点
         -> 按 max_polyline_points 裁剪
         -> 返回 snapshot_id / snapshot_hash / preview_source=planned_glue_snapshot
         -> 返回 preview_kind=glue_points / glue_points / motion_preview / execution_polyline(compat)
   runtime -> gateway -> TcpClient -> Worker
   Worker: 把 plan_id / plan_fingerprint 补回 payload
   Worker -> MainWindow._on_preview_snapshot_completed()

5. HMI 落地快照并渲染
   MainWindow._on_preview_snapshot_completed()
   MainWindow: 校验 snapshot_id / snapshot_hash / preview_source / preview_kind / glue_points
   MainWindow: 保存 _current_plan_id / _current_plan_fingerprint / _preview_source
   MainWindow -> DispensePreviewGate.preview_ready(snapshot)  状态: READY_UNSIGNED
   MainWindow -> _render_runtime_preview_html(preview_source, glue_points, motion_preview, execution_polyline)
   MainWindow: 以 `planned_glue_snapshot + glue_points` 作为成功主语义，优先消费正式 `motion_preview` 运动轨迹预览；缺失时才兼容回退 `execution_polyline`
   MainWindow: 若来源为 `runtime_snapshot` / `mock_synthetic` / 未知来源，则直接按拒绝或诊断路径处理，不进入真实通过口径

6. 用户点击启动
   用户 -> MainWindow._start_production_process()
   MainWindow -> _check_production_preconditions()

7. 若未确认预览
   _check_production_preconditions()
   MainWindow -> DispensePreviewGate.decision_for_start()
   Gate: 返回 CONFIRM_MISSING
   MainWindow -> _confirm_preview_gate()
   MainWindow -> CommandProtocol.dxf_preview_confirm(plan_id, snapshot_hash)
   CommandProtocol -> TcpClient.send_request("dxf.preview.confirm")
   TcpClient -> gateway.HandleDxfPreviewConfirm()
   gateway -> runtime.ConfirmPreview(plan_id, snapshot_hash)
   runtime: plan latest + snapshot_hash 匹配 -> preview_state = CONFIRMED
   runtime -> gateway -> TcpClient -> MainWindow
   MainWindow -> DispensePreviewGate.confirm_current_snapshot()  状态: READY_SIGNED

8. 正式启动
   MainWindow -> DispensePreviewGate.validate_execution_snapshot_hash(_current_plan_fingerprint)
   Gate: hash 一致才放行
   MainWindow -> CommandProtocol.dxf_start_job(plan_id, target_count, plan_fingerprint)
   CommandProtocol -> TcpClient.send_request("dxf.job.start")
   TcpClient -> gateway.HandleDxfJobStart()
   gateway -> runtime.StartJob(plan_id, plan_fingerprint, target_count)
   runtime: 要求 preview_state == CONFIRMED 且 fingerprint 匹配
   runtime -> gateway -> TcpClient -> MainWindow: job_id
```

## 4. 关键分支

### 4.1 参数变更分支

速度或模式变化后，`MainWindow._invalidate_preview_plan()` 会把 gate 置为 `STALE`，并清空 `plan_id`/`plan_fingerprint`，要求重新生成预览。

### 4.2 运行时回同步分支

HMI 在若干状态采集点会置 `_preview_state_resync_pending = True`，随后通过 `_sync_preview_state_from_runtime()` 再次调用 `dxf.preview.snapshot(plan_id)` 同步 runtime 确认态；若 plan 已失效，会降级为“重新生成并确认”。

### 4.3 启动阻断分支

未生成、生成中、失败、已过期、未确认、hash 不一致，都会在 HMI 本地先阻断；安全门、急停等互锁则在 HMI 预检和 runtime `StartJob()` 两侧都拦一次。

### 4.4 数据源识别分支

- `preview_source == planned_glue_snapshot`
  - 仅当 `preview_kind == glue_points` 且 `glue_points` 非空时，HMI 才将其标识为“规划胶点快照”并允许进入真实轨迹验收口径。
  - 若同时存在 `motion_preview.kind == polyline`，HMI 将其作为正式运动轨迹预览路径显示；这不改变 confirm/start gate。
- `preview_source == runtime_snapshot`
  - HMI 将其标识为“旧版 runtime_snapshot”，仅用于诊断，不得进入真实轨迹验收口径。
- `preview_source == mock_synthetic`
  - HMI 必须给出“模拟轨迹，非真实几何”强提示，并作为拒绝/诊断来源处理。

## 5. 对应代码入口

- HMI 主入口：`apps/hmi-app/src/hmi_client/ui/main_window.py`
- Worker 串联 `prepare -> snapshot`：`apps/hmi-app/src/hmi_client/ui/main_window.py`
- HMI 门禁状态机：`apps/hmi-app/src/hmi_client/features/dispense_preview_gate/preview_gate.py`
- 协议封装：`apps/hmi-app/src/hmi_client/client/protocol.py`
- Gateway `snapshot/confirm/start`：`apps/runtime-gateway/transport-gateway/src/tcp/TcpCommandDispatcher.cpp`
- Runtime `prepare/snapshot/confirm/start`：`modules/workflow/application/usecases/dispensing/DispensingWorkflowUseCase.cpp`

## 6. 使用建议

1. 代码走查时，先读第 3 节，再按第 5 节文件入口下钻。
2. 联调定位“预览显示异常”时，先区分是 HMI 对 `motion_preview` 的消费问题，还是上游 `motion_preview` / compat polyline 返回问题。
3. 联调定位“启动被阻断”时，优先检查 gate 状态、`snapshot_hash`、`plan_fingerprint`、`job_id` 和互锁状态。
