# NOISSUE Test Plan - HMI 点胶轨迹点状预览

- 日期：2026-03-21
- 分支：`feat/hmi/NOISSUE-dispense-trajectory-preview-v2`
- 关联文档：
  - `docs/process-model/plans/NOISSUE-scope.md`
  - `docs/process-model/plans/NOISSUE-arch-plan.md`

## 1. 关键路径（必须通过）

1. 主链路正向：`artifact.create -> plan.prepare/preview.snapshot -> 点状预览可见 -> 预览确认 -> job.start`。
2. 门禁生效：未确认预览时，`job.start` 在 HMI 侧被阻断并给出明确提示。
3. 一致性校验：确认后若 `plan_fingerprint` 发生变化，必须阻断启动并提示重新预览。
4. 错误可见性：点集缺失、点集非法、状态冲突时，预览状态进入失败并可追踪。

## 2. 边界场景（必须覆盖）

1. `point_count > 0` 但点集字段缺失。
2. 点集存在但包含非法值（NaN/Inf/越界结构）。
3. 点集为空数组。
4. 预览确认后重新规划，触发 `STALE`。
5. 真实链路互锁触发（门开/急停）时，启动被后端拒绝，HMI 文案正确映射。
6. WebEngine 不可用时，点状预览降级路径仍给出可执行错误反馈，不出现“假成功”。

## 3. 回归基线

1. `DispensePreviewGate` 状态机现有行为不回归。
2. `dxf.plan.prepare` 与 `dxf.job.start` 现有主链路契约不破坏。
3. `dxf.execute` 的 `snapshot_hash` 兼容门禁不破坏。
4. `online/offline smoke` 现有退出码与基础断言不回归。

## 4. 测试命令与预期结果

1. HMI 单元测试  
命令：`D:\Projects\SiligenSuite\apps\hmi-app\scripts\test.ps1`  
预期：`test_dispense_preview_gate.py`、`test_protocol_preview_gate_contract.py` 通过；新增点状预览相关用例通过。

2. 应用层回归（apps 套件）  
命令：`D:\Projects\SiligenSuite\test.ps1 -Suite apps`  
预期：`hmi-app` 的单元测试、offline smoke、online smoke 全部通过。

3. 协议兼容回归  
命令：`D:\Projects\SiligenSuite\test.ps1 -Suite protocol-compatibility`  
预期：`packages/application-contracts` 与 `packages/transport-gateway` 的协议兼容测试通过；新增点集字段 schema 校验通过。

4. 真实链路验收（手工）  
命令：按现场链路执行 `dxf.artifact.create -> dxf.plan.prepare -> dxf.job.start` 对应操作流  
预期：HMI 显示点状预览；确认前不可启动；确认后可启动；执行中与完成后状态收敛正确。

## 5. 可观察性与判定口径

1. 关键日志必须可检索：`preview_requested`、`preview_ready`、`preview_failed`、`preview_confirmed`、`start_blocked_by_preview_gate`、`start_blocked_by_preview_hash`。
2. 每个失败用例必须包含：触发条件、错误文案、门禁状态、是否阻断启动。
3. 真实链路验收通过标准：至少完成一次完整闭环，且无“预览已确认但启动后指纹不一致”问题。

## 6. 退出准则

1. 关键路径 100% 通过。
2. 边界场景无 P0/P1 缺陷残留。
3. 回归基线无破坏。
4. 真实主链路验收通过记录可追溯（包含 plan_id 与 plan_fingerprint）。
