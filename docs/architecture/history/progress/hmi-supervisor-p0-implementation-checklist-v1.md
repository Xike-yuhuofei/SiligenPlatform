# HMI Supervisor P0 最小实施方案与改造清单 V1

更新时间：`2026-03-20`

基线文档（冻结）：

1. `docs/architecture/hmi-supervisor-min-contract-v1.md`
2. `docs/architecture/hmi-online-startup-state-machine-v1.md`

## 1. 现状对齐（As-Is）

当前 `apps/hmi-app` 已具备 online/offline 分流与三段启动链，但“在线真相源”仍分散：

1. 启动真相：`client/startup_sequence.py` 中 `run_launch_sequence` + `StartupWorker`。
2. 运行态真相：`ui/main_window.py` 中 `_connected/_hw_connected` 被多个入口直接写入。
3. 门禁规则：`_require_online_mode()` 仅检查是否 offline，不检查是否 `online_ready`。
4. smoke 真相：`tools/ui_qtest.py` 仍按 `_launch_result + _connected/_hw_connected` 判定；`scripts/online-smoke.ps1` 的 `21` 来自脚本级 mock ready 预探测。

结论：当前结构能跑通，但不满足“Supervisor 是 online_ready 唯一真相源”的冻结要求，存在“假在线”空间。

## 2. 组件职责保留/最小改造/迁移

| 组件 | P0 处理策略 | 保留职责 | 最小改造 | 迁移到 Supervisor 的职责 |
|---|---|---|---|---|
| `StartupWorker` | 最小改造并保留壳层 | 继续作为 UI 线程与后台编排桥接（QThread 生命周期管理） | 改为包装 `SupervisorSession`，发出 `SessionSnapshot` 与阶段事件；保留兼容信号直到 UI 切换完成 | 启动状态机推进、failure_code/failure_stage 生成、recoverable 判定、恢复动作入口 |
| `BackendManager` | 最小改造 | 进程拉起/停止、ready 探测、launch spec 装载 | 返回结构化结果（含 failure_code 候选）而非仅 `(bool, msg)`；保留现有启动契约与环境变量 | failure_code 归一化与状态映射由 Supervisor 统一决策 |
| `TcpClient` | 保留为传输层 | TCP 建连、请求响应、收包线程 | 增补可观测错误语义（连接失败原因）供 Supervisor 消费；不引入业务状态机 | TCP 阶段状态推进（connecting/ready/failed）与重试策略 |
| HMI 在线判定与门禁（`main_window.py`） | 最小改造 | UI 展示、按钮使能、用户反馈 | 由“读 `_connected/_hw_connected`”切换为“读 Supervisor 快照”；`_require_online_mode` 升级为基于 `online_ready` 的统一门禁 | 在线真相判定、阶段失败归因、恢复动作编排 |

## 3. P0 最小可行实现的新增与改动清单

## 3.1 计划新增文件（P0 必需）

1. `apps/hmi-app/src/hmi_client/client/supervisor_contract.py`
- 定义 `SessionSnapshot`、`SessionState`、`FailureCode`、`FailureStage`、`RecoveryAction`。

2. `apps/hmi-app/src/hmi_client/client/supervisor_session.py`
- 封装 `backend -> TCP -> hardware` 状态机推进。
- 输出阶段事件、失败映射、recoverable 判定。

3. `apps/hmi-app/tests/unit/test_supervisor_session.py`
- 对阶段转移、failure_code 映射、recoverable 判定做纯逻辑单测。

4. `apps/hmi-app/tests/unit/test_supervisor_contract.py`
- 对契约字段完整性与约束做单测。

## 3.2 计划修改文件（P0 必需）

1. `apps/hmi-app/src/hmi_client/client/startup_sequence.py`
- 从“直接实现状态机”改为“兼容壳层 + 调用 SupervisorSession”。

2. `apps/hmi-app/src/hmi_client/client/backend_manager.py`
- 增补结构化错误输出（最小变更，不改启动契约输入面）。

3. `apps/hmi-app/src/hmi_client/client/__init__.py`
- 导出 Supervisor 契约与会话对象（保持旧导出兼容）。

4. `apps/hmi-app/src/hmi_client/ui/main_window.py`
- 门禁、状态栏、错误提示改为消费 `SessionSnapshot`。
- 移除 `_connected/_hw_connected` 作为真相源的写入路径（可保留为派生缓存，但不得作为判定输入）。

5. `apps/hmi-app/src/hmi_client/tools/ui_qtest.py`
- 在线断言从 `_launch_result.phase==ready` + `_connected/_hw_connected` 改为 `online_ready` 判定。
- 增加 failure_code/failure_stage 断言钩子。

6. `apps/hmi-app/scripts/online-smoke.ps1`
- 增加 Supervisor 观测输出采集与 `21` 诊断映射（`SUP_BACKEND_READY_TIMEOUT`）。

7. `apps/hmi-app/scripts/verify-online-ready-timeout.ps1`
- 从“匹配脚本文案超时”调整为“匹配退出码21 + failure_code 映射”。

8. `apps/hmi-app/docs/smoke-exit-codes.md`
- 最小补充“21 对应 failure_code 观测要求”，不改现有退出码定义。

## 3.3 P0 可延期（明确不做）

1. 多客户端共享会话与会话租约。
2. IPC 通用抽象层/服务总线。
3. 会话历史持久化与回放。
4. HMI 全量架构重排（仅改门禁与状态消费路径）。

## 4. 改动点与问题映射

| 改动点 | 解决的问题 |
|---|---|
| `supervisor_contract.py` | 消除状态语义分裂，避免 ready 判定口径不一致 |
| `supervisor_session.py` | 将失败映射、recoverable、恢复动作集中，解决“失败不可诊断/恢复路径不清” |
| `startup_sequence.py` 壳层化 | 兼容现有调用点，避免一次性重构导致回归风险 |
| `main_window.py` 门禁切换 | 消除 `_connected/_hw_connected` 导致的“假在线” |
| `ui_qtest.py` 断言升级 | 让 GUI smoke 验证 Supervisor 真相，而不是 UI 内部推断 |
| `online-smoke.ps1` 与 `verify-online-ready-timeout.ps1` 接线 | 保持退出码契约不变前提下，补齐 failure_code/failure_stage 诊断链路 |

## 5. smoke / 退出码 / 可观测性最小接线

P0 最小接线目标：不改退出码定义，只补“诊断可观测”。

1. `offline-smoke.ps1`：继续返回 `0/10/11`，新增校验点为 `mode=offline` 且在线能力门禁关闭。
2. `online-smoke.ps1`：继续返回 `0/10/11/20/21`，但输出中需带 `failure_code/failure_stage` 观测结果。
3. `verify-online-ready-timeout.ps1`：`21` 必须可映射为 `SUP_BACKEND_READY_TIMEOUT`。
4. HMI/UI smoke 观察点：
- `label-launch-mode`
- `session_state`
- `failure_code`
- `failure_stage`
- 在线按钮使能状态

## 6. 建议实施顺序（先恢复稳定运行能力）

## 步骤 1：冻结契约落点（代码侧）

- 目标：在代码层引入 `SessionSnapshot` 与 failure 枚举，形成统一类型基线。
- 涉及文件：
  - 新增 `client/supervisor_contract.py`
  - 修改 `client/__init__.py`
- 风险点：旧代码继续直接使用布尔标志，导致双口径并存。
- 完成判据：所有新旧调用可引用同一契约类型；不破坏现有单测加载。

## 步骤 2：落地最小 Supervisor 会话状态机

- 目标：把 backend->TCP->hardware 状态推进与失败映射从 UI 抽离。
- 涉及文件：
  - 新增 `client/supervisor_session.py`
  - 修改 `client/startup_sequence.py`
  - 修改 `client/backend_manager.py`
- 风险点：兼容层处理不当会破坏现有 `StartupWorker` 行为。
- 完成判据：可在单测中覆盖阶段流转、超时映射、recoverable 判定。

## 步骤 3：HMI 切换到 Supervisor 真相源

- 目标：门禁与状态栏完全基于 `SessionSnapshot`，禁止 `_connected/_hw_connected` 直判。
- 涉及文件：
  - 修改 `ui/main_window.py`
- 风险点：按钮使能逻辑变化导致离线/在线体验回归。
- 完成判据：
  - 仅 `online_ready` 放行在线动作。
  - `starting/degraded/failed/stopping` 全部阻断在线动作。

## 步骤 4：smoke 与可观测性接线

- 目标：保持退出码不变，补齐 failure_code/failure_stage 观测。
- 涉及文件：
  - 修改 `tools/ui_qtest.py`
  - 修改 `scripts/online-smoke.ps1`
  - 修改 `scripts/verify-online-ready-timeout.ps1`
  - 最小更新 `docs/smoke-exit-codes.md`
- 风险点：现有 `online-smoke` 的脚本级 ready 预探测与 Supervisor ready 语义冲突。
- 完成判据：
  - `online-smoke` 仍输出既有退出码。
  - `21` 可观测到 `SUP_BACKEND_READY_TIMEOUT`。

## 步骤 5：P0 回归收口

- 目标：验证“稳定运行能力恢复”且无方案 C 漂移。
- 涉及文件：
  - 新增 `tests/unit/test_supervisor_session.py`
  - 新增 `tests/unit/test_supervisor_contract.py`
  - 视需要最小修改 `tests/unit/test_startup_sequence.py`
- 风险点：测试覆盖不完整导致隐式假在线回流。
- 完成判据：
  - 单测通过。
  - `offline-smoke`、`online-smoke`、`verify-online-ready-timeout` 三条链路通过。
  - 未引入多客户端/通用 IPC/持久化等延期项。

## 7. 与冻结文档的结构落差与最小修正建议

当前明显落差：

1. HMI 仍直接持有并写入 `_connected/_hw_connected` 真相。
2. `_require_online_mode()` 仅判定 offline，不判定 `online_ready`。
3. `online-smoke` 的 `21` 目前来源于脚本级 mock ready 检查，未经过 Supervisor failure_code 语义。

最小修正建议（不扩大范围）：

1. 保留现有 `StartupWorker` 作为兼容壳层，先接入 SupervisorSession，避免一次性替换 UI 线程模型。
2. 先替换门禁判定函数（单点切换到 `online_ready`），再逐步清理 `_connected/_hw_connected` 写入路径。
3. 保持退出码不变，只补诊断采集和 failure_code 映射，不改 smoke 脚本总体流程边界。

## 8. P1 稳定收口进展（2026-03-20）

在不扩展方案 C 范围的前提下，完成以下收口项：

1. [x] UI 移除 `_connected/_hw_connected` 真相缓存，门禁与状态展示统一基于 `SessionSnapshot`。
2. [x] `LaunchResult` 增加快照对齐逻辑，避免与 `SessionSnapshot` 双口径漂移。
3. [x] 运行态退化映射集中到 `SupervisorSession.detect_runtime_degradation`。
4. [x] `online-smoke.ps1` 在 `ExpectFailure*` 场景强制走 Supervisor 注入路径，并要求 `SUPERVISOR_DIAG` 观测成功态 `online_ready=true`。
5. [x] 文档补齐上述 smoke 约束，维持退出码契约不变。

## 9. P2 稳定化收口进展（2026-03-20）

在不改变退出码契约、不引入方案 C 扩展的前提下，完成以下最小收口项：

1. [x] `SupervisorSession` 引入 `SupervisorPolicy`，将 backend/tcp/hardware 超时从硬编码升级为可配置策略（默认值保持 `5s/3s/15s`）。
2. [x] `StartupWorker` 与 `RecoveryWorker` 共享同一 Supervisor 策略实例，避免启动链与恢复链超时口径漂移。
3. [x] 运行态退化新增 `stage_failed` 事件闭环（`detect_runtime_degradation_with_event`），并在 UI 侧统一落入 `_supervisor_stage_events`。
4. [x] 生产控制路径补齐门禁：`_on_production_pause/_resume/_stop` 与自动续跑分支均要求 `online_ready`。
5. [x] recovery smoke 断言升级：必须同时观测到 `stage_failed@tcp_ready`、`recovery_invoked`、`stage_succeeded@online_ready` 与最终 `SUPERVISOR_DIAG ... online_ready=true`。
