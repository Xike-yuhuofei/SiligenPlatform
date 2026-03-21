# NOISSUE Arch Plan - HMI 点胶轨迹点状预览

- 日期：2026-03-21
- 分支：`feat/hmi/NOISSUE-dispense-trajectory-preview-v2`
- 输入基线：`docs/process-model/plans/NOISSUE-scope.md`

## 1. 方案结论（冻结）

1. 预览权威来源采用“真实主链路规划快照”，不使用本地二次推导作为门禁依据。
2. HMI 预览形态改为点状几何路径（模拟胶点分布），并继续复用现有确认门禁与指纹校验。
3. 本轮属于“增量架构调整”，不做跨域重构；但需要协议层补齐点状数据字段，属于外部契约变更。

## 2. 现状事实

1. HMI 当前主流程通过 `dxf.plan.prepare` 获取 `plan_id/plan_fingerprint/point_count` 等摘要字段。
2. HMI 当前预览渲染使用 `engineering-data` 生成 HTML 文件，数据源是本地 DXF 路径，不是运行时快照点集。
3. `dxf.preview.snapshot` 在 gateway 侧当前等价转发 `dxf.plan.prepare`，未输出轨迹点集。
4. `DispensePreviewGate` 门禁状态机已存在且可复用，不需要新增并行状态机。

## 3. 受影响目录与文件类型

1. `apps/hmi-app/src/hmi_client/ui/*.py`（UI 展示、门禁触发、状态文案）
2. `apps/hmi-app/src/hmi_client/client/*.py`（协议调用与返回字段解析）
3. `apps/hmi-app/src/hmi_client/tools/*.py`（mock 行为与联调支持）
4. `apps/hmi-app/tests/unit/*.py`（协议契约、门禁逻辑、UI 预览行为）
5. `packages/transport-gateway/src/tcp/*.cpp|*.h`（命令处理与响应组装）
6. `packages/process-runtime-core/src/application/usecases/dispensing/*.h|*.cpp`（计划响应字段承载能力）
7. `packages/application-contracts/commands/*.json`（协议 schema）
8. `packages/application-contracts/fixtures/**/*.json`（协议样例）
9. `packages/application-contracts/tests/*.py`（协议兼容回归）

## 4. 数据流 / 调用流

```text
HMI(MainWindow)
  -> dxf.artifact.create
  -> dxf.plan.prepare / dxf.preview.snapshot
      -> Transport Gateway(TcpCommandDispatcher)
         -> DispensingWorkflowUseCase::PreparePlan
         -> 返回 plan_id + plan_fingerprint + 点状几何数据
  -> HMI 点状渲染 + 预览确认签核(PreviewGate READY_SIGNED)
  -> dxf.job.start(plan_id, plan_fingerprint)
      -> Gateway 对计划指纹做一致性校验
      -> Runtime 执行
```

## 5. 状态机与生命周期影响

1. 保持既有 `DIRTY -> GENERATING -> READY_UNSIGNED -> READY_SIGNED` 门禁主干。
2. 变更点在于 `preview_ready` 的数据语义：从“外部 HTML 预览可用”切换为“点状快照已加载并可核对”。
3. `STALE` 判定继续由输入变更触发，且必须清空确认签名。
4. `FAILED` 场景新增契约缺失与点集非法两类错误映射。

## 6. 外部契约与接口边界

1. 需要在 `dxf.plan.prepare` 或 `dxf.preview.snapshot` 响应中稳定提供点状几何数据字段。
2. 该字段需与 `plan_fingerprint/snapshot_hash` 同源，避免“点集与执行计划不一致”。
3. `dxf.job.start(plan_id, plan_fingerprint)` 契约保持不变，作为执行门禁的最终校验入口。
4. 文件格式层面不新增持久化文件作为主路径依赖；点集通过协议响应承载。

## 7. 失败模式与补救策略

1. 失败模式：规划成功但无点集字段。  
补救：预览进入 `FAILED`，禁止启动，并反馈“输入不满足点状预览前提”。

2. 失败模式：点集为空或坐标非法。  
补救：预览失败并记录日志上下文（plan_id、fingerprint、字段摘要）。

3. 失败模式：预览确认后计划已更新（指纹漂移）。  
补救：状态转 `STALE`，要求重新预览与确认。

4. 失败模式：主链路可执行但 HMI 渲染性能退化。  
补救：对展示点集做上限与降采样策略，但不得改变用于门禁校验的计划指纹。

5. 失败模式：真实链路联调不可用。  
补救：允许临时使用 mock 做开发验证，但不作为最终验收通过条件。

## 8. 发布与回滚策略

1. 发布策略：按“契约 -> gateway/runtime -> HMI”顺序合入，保证上游字段先可用。
2. 兼容策略：在契约未升级完成前，HMI 对缺失字段执行明确失败而非静默回退。
3. 回滚策略：若上线后出现主链路阻断，回滚到“仅摘要预览 + 原门禁”版本，但保持 `plan_fingerprint` 校验不回退。
4. 观测策略：发布后必须监控 `preview_failed`、`start_blocked_by_preview_gate`、`start_blocked_by_preview_hash` 频次。
