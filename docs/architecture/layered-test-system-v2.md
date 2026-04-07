# SiligenPlatform 分层测试体系 v2

更新时间：`2026-04-02`

## 1. 正式分层定义

全仓唯一正式分层如下：

| 层级 | 正式名称 | 责任 | 典型输入 | 典型输出 | 阻断条件 |
|---|---|---|---|---|---|
| `L0` | 静态与构建门禁 | 根级 build/test/CI 入口有效、Python 静态类型、loose mock 禁止、关键脚本与资产完整性 | 仓库源码、根级脚本、测试资产 | `tests/reports/static/*`、workspace gate summary | 任一静态检查、根入口或关键资产检查失败 |
| `L1` | 单元测试 | 局部规则、状态推进、错误传播、关键 side effect | 单模块输入、严格受约束 mock/fake | unit report、断言日志 | preview/gateway/startup 等核心单测失败 |
| `L2` | 模块契约测试 | 冻结 `CommandProtocol`、preview request/response、gateway launch、schema/事件契约 | 契约定义、consumer 期望、fake/stub/adapter | contract report、compatibility evidence | 参数签名漂移、schema 兼容失败、fake 宽于真实实现 |
| `L3` | 集成测试 | 离线主发现层，覆盖 DXF 导入、preview、工艺/执行载荷、网关启动/在线连接、preflight 等跨模块链路 | canonical samples、integration scenarios | integration report、样本与命令证据 | 任一主链不通、状态机不一致、样本无法复放 |
| `L4` | 模拟全链路 E2E | simulated-line 或等效模拟环境下的完整主路径与关键异常路径 | simulated-line 样本、故障注入计划 | `case-index.json`、`validation-evidence-bundle.json`、`evidence-links.md` | 模拟执行主链失败、异常路径无证据 |
| `L5` | HIL 闭环测试 | 在 `L0-L4` 已通过后，对真实设备做最小闭环确认 | offline prerequisite report、operator/safety admission | HIL summary、admission bundle、operator evidence | admission/preflight 失败或任一闭环动作失败 |
| `L6` | 性能与稳定性测试 | 大样本、抖动、长稳、资源释放、趋势对比 | performance samples、threshold config、必要时复用 L3/L4 样本 | performance report、threshold gate、trend evidence | 超阈值、长稳回归、资源泄漏趋势异常 |

## 2. lane 与 L0-L6 的正式映射

| lane | 覆盖层 | 角色 | 说明 |
|---|---|---|---|
| `quick-gate` | `L0`、`L1` 核心单元、`L2` 核心契约 | 本地与 PR 快速阻断 | 拦截类型错误、边界漂移、关键单测/契约失败 |
| `full-offline-gate` | `L0`、`L1`、`L2`、`L3`、`L4` 最小主路径 smoke | 机台前主发现层 | 不依赖真实机台，优先离线暴露主链问题 |
| `nightly-performance` | `L6`，必要时复用 `L3/L4` 主样本 | 性能回归与长稳趋势 | 正式 blocking 以阈值/趋势为准 |
| `limited-hil` | `L5`，必要时附带少量 `L6` 稳定性样本 | 真实设备闭环确认 | 不替代离线主发现层 |

## 3. 主链路与横向能力映射

| 链路 / 能力 | L0 | L1 | L2 | L3 | L4 | L5 | L6 |
|---|---|---|---|---|---|---|---|
| DXF 导入与工程数据摄取 | 根级入口/类型 | 导入默认路径 | 工程数据契约 | `run_engineering_regression.py` | 间接被 simulated-line 消费 | 最小真实样本闭环 | 大样本导入耗时 |
| DXF 预览链 | loose mock / 签名 | preview session / worker / gate | preview request/response | `run_preview_flow_regression.py` | preview 相关主路径证据随 simulated-line 出包 | `run_real_dxf_preview_snapshot.py` 等闭环确认 | preview 耗时与抖动 |
| 工艺规划 / 打包 / 执行载荷 | 构建与入口 | 局部规则 | 输出契约 | engineering regression | simulated-line 执行 | 最小执行闭环 | 打包耗时 |
| 运动规划 / 插补 / 时间语义 | 构建与静态 | 局部单测 | 输出 schema | integration / simulated input replay | simulated-line | 真实设备闭环 | 长稳与资源趋势 |
| 网关启动 / 在线连接 / 状态机 | gateway launch gate | startup sequence 单测 | launch contract | `online-smoke.ps1` integration `run_tcp_precondition_matrix.py` | simulated-line readiness/abort hooks | HIL admission + closed loop | startup timing |
| 报警 / 互锁 / 故障恢复 | 脚本完整性 | 判定/恢复规则 | 事件 schema | first-layer / integration smoke | simulated-line fault hooks | 报警/恢复闭环 | 重复暂停/恢复稳定性 |
| 配方 / 配置 / 版本兼容 | 配置入口 | 默认值/兼容转换 | schema 契约 | integration replay | smoke 覆盖有限 | 待补最小 HIL 回放 | 待补长期趋势 |
| 启动作业 / 运行期执行 | 静态与构建 | 局部 job 控制 | execution contract | online smoke / engineering regression | simulated-line | HIL execution closed loop | execution timing |
| 停止 / 暂停 / 恢复 / 复位 | 静态与类型 | 局部逻辑 | 事件/状态契约 | integration smoke | simulated-line hooks | HIL case matrix | repeated pause/resume |
| 部署 / 启动脚本 / 环境准备 | 根级入口 | 不适用 | launch contract | gateway startup integration | 不适用 | operator admission | startup trend |
| 静态类型与边界治理 | `L0` 主承载 | 辅助 | 辅助 | - | - | - | - |
| 模块契约冻结 | - | 辅助 | `L2` 主承载 | 辅助 | 辅助 | 引用前置通过 | 趋势引用 |
| 故障注入与恢复资产 | 资产完整性 | 局部失败分支 | fault schema | integration faults | simulated faults | HIL faults | performance stress |
| 样本库 / 黄金基线库 | 资产完整性 | - | baseline contract | sample replay | simulated samples | HIL sample admission | perf samples |
| 证据与可观测性 | static report | unit/assert log | contract evidence | integration summary | evidence bundle | admission + HIL bundle | threshold / trend bundle |
| 性能与长稳 | - | - | - | 可复用样本 | 可复用主路径 | 少量硬件样本 | `L6` 主承载 |
| 人工探索式工作流检查表 | 资产完整性 | - | checklist contract | 引用 | 引用 | operator checklist | 引用 |

## 4. L0 责任边界

`L0` 当前正式接线为：

- `pyrightconfig.json`
- `scripts/validation/run-pyright-gate.ps1`
- `scripts/validation/check_no_loose_mock.py`
- 根级 `build.ps1` / `test.ps1` / `ci.ps1`
- `scripts/validation/run-local-validation-gate.ps1`

核心边界包括：

- `CommandProtocol`
- gateway client / adapter
- preview snapshot 边界
- gateway launch / startup contract
- 运行期执行边界
- 关键 request / response schema
- 与 HMI 在线状态直接耦合的协议对象

允许：

- `create_autospec(..., spec_set=True)`
- `patch(..., autospec=True)`
- `Mock(spec=...)`
- `Mock(spec_set=...)`

禁止：

- 裸 `Mock()`
- 裸 `MagicMock()`
- 不带 `autospec/spec/spec_set` 的核心边界调用拦截
- 用 `*args, **kwargs` 通吃核心边界 fake/stub

## 5. 机台前阻断原则

- `L0` 失败即阻断 `quick-gate`
- `L0` 不通过，禁止进入 `L3/L4/L5`
- `L5` 只允许在 `L0-L4` 全绿后执行
- `L5` 的角色是闭环确认，不是低级 bug 主发现层

## 6. 证据与报告

正式报告根：

- `tests/reports/static/`
- `tests/reports/integration/`
- `tests/reports/e2e/`
- `tests/reports/performance/`

当前正式证据对象：

- `workspace-validation.json` / `workspace-validation.md`
- `case-index.json`
- `validation-evidence-bundle.json`
- `evidence-links.md`
- `pyright-report.*`
- `no-loose-mock-report.*`

## 7. 伪覆盖判定口径

以下直接视为伪覆盖：

- Mock 接受任意参数，导致签名错误无法暴露
- 只断言“被调用过”，不校验参数、状态、错误传播或 side effect
- fake/stub 比真实实现更宽松
- 只测 happy path，不测失败分支
- 测试通过但无可定位证据
- GUI 测试只验控件存在，不验主路径状态变化
- `connect` 只验无 error，不验 `result.connected` 与 `status.connected`
- HIL 报告不声明已通过的前置离线层
