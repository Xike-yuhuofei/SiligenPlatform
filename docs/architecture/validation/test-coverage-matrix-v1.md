# 测试覆盖矩阵 v1

更新时间：`2026-04-02`

说明：

- `已有资产` 只列当前正式纳入 gate 或 authority path 的资产。
- `缺失资产` 表示本轮后仍未完全落地的风险点。
- `伪覆盖资产` 表示历史上存在但不能充当对应层正式覆盖的资产。
- `owner` 先保留占位，后续应补成明确模块 owner。

| 链路 | owner | L0 | L1 | L2 | L3 | L4 | L5 | L6 | gate | 证据 | 已有资产 | 缺失资产 | 伪覆盖资产 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| DXF 导入与工程数据摄取 | `TBD:dxf-geometry` | `pyright` / loose mock | 局部单测待补 | `test_engineering_data_compatibility.py` | `run_engineering_regression.py` | 间接覆盖 | 待补最小闭环 | 大样本导入待扩 | `full-offline-gate` | workspace validation / integration summary | `tests/contracts/test_engineering_data_compatibility.py` `tests/integration/scenarios/run_engineering_regression.py` | L1 入口单测、L5 最小闭环、L6 导入阈值 | 仅靠 HIL dry-run 观察无法替代 L3 |
| DXF 预览链 | `TBD:hmi` | `pyright` / loose mock | preview gate / worker / session 单测 | `test_protocol_preview_gate_contract.py` | `run_preview_flow_regression.py` | simulated-line 间接覆盖 | `run_real_dxf_preview_snapshot.py` | `collect_dxf_preview_profiles.py` | `quick-gate` / `full-offline-gate` / `nightly-performance` | static + integration + preview/perf bundle | `apps/hmi-app/tests/unit/test_protocol_preview_gate_contract.py` `apps/hmi-app/tests/unit/test_preview_session_worker.py` `modules/hmi-application/tests/unit/test_preview_session.py` `tests/integration/scenarios/run_preview_flow_regression.py` | 专门的 L4 preview 异常路径矩阵仍待补 | `tests/integration/scenarios/first-layer/test_real_preview_snapshot_geometry.py` 依赖真实环境，不能充当离线 L3 |
| 工艺规划 / 打包 / 执行载荷 | `TBD:workflow` | 根级 gate | 局部规则待补 | protocol / contract 现有覆盖有限 | `run_engineering_regression.py` | simulated-line 主路径 | HIL closed loop | 执行载荷耗时待扩 | `full-offline-gate` | integration summary / e2e bundle | engineering regression + simulated-line | L1/L2 精细契约、L6 打包指标 | 仅 smoke 成功而无载荷证据视为伪覆盖 |
| 运动规划 / 插补 / 时间语义 | `TBD:runtime-execution` | 根级 gate | C++ unit 通过 root suite | contract 覆盖有限 | simulated input replay | simulated-line | HIL case matrix | following error / stability | `full-offline-gate` / `nightly-performance` | bundle + performance trend | simulated-line + perf profiler | 更明确的 L2 输出契约 | 只看 UI 状态不看轨迹/时间语义 |
| 网关启动 / 在线连接 / 状态机 | `TBD:gateway` | launch contract / static gate | `test_startup_sequence.py` 等 | gateway launch contract | `online-smoke.ps1` `run_tcp_precondition_matrix.py` | simulated-line readiness hooks | HIL admission + closed loop | startup timing | `full-offline-gate` / `limited-hil` | workspace validation / HIL summary | `apps/hmi-app/tests/unit/test_startup_sequence.py` `apps/hmi-app/tests/unit/test_gateway_launch.py` `apps/hmi-app/tests/unit/test_backend_manager.py` `apps/hmi-app/scripts/online-smoke.ps1` `tests/integration/scenarios/first-layer/run_tcp_precondition_matrix.py` | backend ready -> tcp ready -> hardware probe 的更细分样本仍可继续扩，但主 admission 链已纳入 L3 | 仅能启动进程但不校验 `result.connected` / `status.connected` |
| 报警 / 互锁 / 故障恢复 | `TBD:runtime-execution` | static gate | 规则单测已有局部覆盖 | 事件契约待补 | first-layer / integration smoke | simulated-line fault hooks | HIL alarm/recovery | 重复恢复稳定性待扩 | `full-offline-gate` / `limited-hil` | integration summary / HIL bundle | `tests/integration/scenarios/first-layer/*` `tests/e2e/simulated-line/run_simulated_line.py` | 统一 L2 事件 schema、L6 长稳 | HIL 首次发现互锁/参数错配 |
| 配方 / 配置 / 版本兼容 | `TBD:recipe-config` | static gate | 默认值/兼容转换待补 | schema 契约局部覆盖 | `run_recipe_config_compatibility.py` | smoke 待补 | HIL 回放待补 | 长稳待补 | `quick-gate` / `full-offline-gate` | contract report + integration summary | 部分 recipe APIs 在 `CommandProtocol` `tests/integration/scenarios/run_recipe_config_compatibility.py` | L1 默认值/兼容转换、L4 smoke、L5 回放、L6 长稳仍缺 | 只验 schema 文件存在、不验兼容迁移 |
| 启动作业 / 运行期执行 | `TBD:runtime-execution` | static gate | job 控制局部逻辑 | execution contract | online smoke / engineering regression | simulated-line | HIL closed loop | execution timing | `full-offline-gate` / `limited-hil` / `nightly-performance` | e2e bundle / perf report | `CommandProtocol.dxf_start_job` 契约、simulated-line、HIL closed loop | 更细 L3 preflight -> start job 集成链 | 只断言按钮可点、不验状态推进 |
| 停止 / 暂停 / 恢复 / 复位 | `TBD:runtime-execution` | static gate | 局部逻辑已有 | 契约局部覆盖 | integration smoke 待补强 | simulated-line hooks | HIL case matrix | repeated cycles 待扩 | `full-offline-gate` / `limited-hil` / `nightly-performance` | HIL matrix / perf report | `run_case_matrix.py` + existing unit logic | L3 离线矩阵、L6 repeated cycles | HIL 无前置离线声明 |
| 部署 / 启动脚本 / 环境准备 | `TBD:infra` | 根级入口 / build gate | 不适用 | launch contract | online smoke | 不适用 | operator admission | startup trend | `quick-gate` / `full-offline-gate` / `limited-hil` | static report / admission report | root scripts + launch contract guards | 环境准备 checklist 与更多自动校验 | 只看脚本存在，不验实际参数与报告 |

## 伪覆盖清单（当前已识别）

| 文件 | 问题类型 | 风险 | 处理方式 |
|---|---|---|---|
| `tests/integration/scenarios/first-layer/test_real_preview_snapshot_geometry.py` | 依赖真实环境，不能充当离线 L3 主发现层 | 机台前才暴露 preview 主链问题 | 保留为 HIL/联机辅助资产，不计入离线主覆盖 |
| `apps/hmi-app/tests/unit/test_preview_flow_integration.py` | 物理位置在 unit，但语义是跨组件主链 | 若只算 L1 会高估单元覆盖 | 基于同链路新增 `tests/integration/scenarios/run_preview_flow_regression.py` |
| 历史宽松 patch / Mock 用法 | 签名错误可能被 fake 吞掉 | 参数漂移延后到联机/HIL 才暴露 | 已接入 `scripts/validation/check_no_loose_mock.py` 并加固关键测试 |
| 历史 `connect` 仅检查“无 error” | gateway 返回 `result.connected=false` 或后续 `status.connected=false` 时会形成假在线 | `home` / `preview` / `start job` 链路被延后到更深层才暴露 | 已将 strict admission 收敛到 `runtime_gateway_harness` 并接入 `tcp-precondition-matrix` 等 runner |
