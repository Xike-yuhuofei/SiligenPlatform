# NOISSUE Scope - HMI 点胶轨迹预览（点状几何路径）

- 状态：Frozen
- 日期：2026-03-21
- 分支：`feat/hmi/NOISSUE-dispense-trajectory-preview-v2`
- 阶段：`ss-scope-clarify -> ss-arch-plan` 交接基线

## 1. 背景与目标

当前主链路已经收敛为 `dxf.artifact.create -> dxf.plan.prepare -> dxf.job.start`。  
本任务目标是在 HMI 中提供“已规划轨迹”的执行前预览，并且预览形态必须是点状几何路径，用于模拟真实胶点分布，支持操作者在启动前做可视化核对与签核。

## 2. In Scope

1. HMI 在“规划成功且未启动执行”状态下，展示已规划轨迹的点状几何路径。
2. 预览数据来源固定为“当前任务最新规划成功快照”，不允许多来源混用。
3. 预览与执行保持语义一致：坐标系、单位、点序一致；预览侧不改写轨迹语义。
4. 启动前门禁保持有效：未完成预览确认不得启动生产。
5. 错误反馈必须显式可见：至少覆盖 `无轨迹`、`轨迹非法`、`轨迹过期`、`状态冲突`。
6. 验收主对象不以离线/仿真链路为主，应基于真实主链路或与其等价的集成链路。

## 3. Out of Scope

1. 轨迹规划算法、插补算法、工艺参数优化。
2. 胶量/胶形/铺展等工艺质量签收。
3. 多工件并发、多任务调度、异常恢复全覆盖。
4. 与本需求无关的 HMI 页面重构、视觉风格重做。

## 4. 受影响模块清单

1. `apps/hmi-app/src/hmi_client/ui/main_window.py`
2. `apps/hmi-app/src/hmi_client/client/protocol.py`
3. `apps/hmi-app/src/hmi_client/features/dispense_preview_gate/preview_gate.py`
4. `apps/hmi-app/src/hmi_client/tools/mock_server.py`
5. `apps/hmi-app/tests/unit/test_protocol_preview_gate_contract.py`
6. `apps/hmi-app/tests/unit/test_dispense_preview_gate.py`
7. `packages/transport-gateway/src/tcp/TcpCommandDispatcher.cpp`
8. `packages/transport-gateway/src/tcp/TcpCommandDispatcher.h`
9. `packages/process-runtime-core/src/application/usecases/dispensing/DispensingWorkflowUseCase.h`
10. `packages/process-runtime-core/src/application/usecases/dispensing/DispensingWorkflowUseCase.cpp`
11. `packages/application-contracts/commands/dxf.command-set.json`
12. `packages/application-contracts/fixtures/responses/dxf.preview.snapshot.success.json`
13. `packages/application-contracts/tests/test_protocol_compatibility.py`

说明：4-13 是否全部进入改动取决于“点状数据从何处提供”的最终契约决策；本文件仅冻结范围，不强行绑定实现路径。

## 5. 输入与输出

- 输入：
  - `artifact_id`
  - 规划成功快照标识（`plan_id/snapshot_id`）
  - 规划指纹（`plan_fingerprint/snapshot_hash`）
  - 轨迹点状几何数据（用于模拟胶点分布）
- 输出：
  - HMI 点状轨迹预览
  - 预览确认状态（可启动/不可启动）
  - 失败时可操作错误信息

## 6. 验收标准（可测试、可观察）

1. 在主链路成功规划后，HMI 可展示点状几何路径预览，不是仅线段轮廓。
2. 未确认预览时，生产启动被门禁拦截并给出明确原因。
3. 预览确认后，且指纹匹配时，允许启动；指纹不匹配时必须拦截。
4. 预览失败场景必须显式提示，不允许静默失败或空白成功态。
5. 在真实主链路环境下完成至少一次“规划 -> 点状预览 -> 确认 -> 启动”的端到端验证。

## 7. 风险与前置依赖

1. 当前 `dxf.plan.prepare` 响应不含点状几何数据，存在契约缺口。
2. 当前 `dxf.preview.snapshot` 在 gateway 侧等价转发 `dxf.plan.prepare`，与“点状预览”目标不一致。
3. 点数规模较大时存在渲染性能风险（CPU 占用、UI 卡顿、内存峰值）。
4. 主链路验收依赖真实环境可用性（设备、互锁、网络）。
5. 若点状数据与执行指纹非同源，存在“看见的不是将要执行的”风险。

## 8. 明确不做项

1. 不在本轮承诺工艺质量达标。
2. 不在本轮引入多版本并发预览管理。
3. 不在本轮扩展到历史任务回放与批量比较功能。
4. 不在本轮做协议层大规模重构。
