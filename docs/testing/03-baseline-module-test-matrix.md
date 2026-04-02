# 03 Baseline Module Test Matrix

## 1. Purpose

### 1.1 文档目的
本文档冻结 `SiligenPlatform` 各模块的最小测试面，统一：
- 模块分组方式
- 测试类型矩阵
- 最小样本集
- 故障注入字典
- 模块优先级与建设顺序

### 1.2 与其他基准文档的关系
本文档建立在以下文档之上：
- `01-baseline-architecture.md`：定义测试分层与总原则
- `02-baseline-directory-and-ownership.md`：定义目录与 owner 边界
- `05-baseline-suite-label-ci.md`：定义 suite / label / CI profile
- `06-baseline-simulated-line-and-hil.md`：定义 simulated-line 与 HIL 边界

本文档不规定具体测试实现细节，但规定“每个模块至少应拥有哪些正式测试面”。

---

## 2. Module Grouping Strategy

### 2.1 产物型模块
产物型模块的核心职责是：
- 接收输入
- 执行业务转换
- 输出稳定且可被下游消费的中间产物

该类模块的主风险为：
- 输出语义漂移
- 字段或结构漂移
- 边界输入处理错误
- 数值规则、顺序规则、组装规则错误

该类模块应优先建设：
- `unit`
- `contracts`
- `golden`
- 必要时的 `compatibility`

### 2.2 运行时控制型模块
运行时控制型模块的核心职责是：
- 状态驱动
- 时序驱动
- 故障恢复
- 执行控制

该类模块的主风险为：
- 隐式状态
- 非法状态迁移
- 重入/幂等问题
- 故障后错误恢复
- 对外状态暴露与真实状态不一致

该类模块应优先建设：
- `unit`
- `contracts`
- `state-machine`
- `fault-injection`
- `recovery`

### 2.3 观测与交互型模块
观测与交互型模块的核心职责是：
- 将真实系统状态正确投影给用户或上层观测者
- 不发明与真实系统脱钩的虚假状态

该类模块的主风险为：
- 假在线
- 假完成
- 假恢复
- 状态投影不一致
- 观测证据与事实链不一致

该类模块应优先建设：
- `contracts`
- `presenter/view-model`
- `startup-state`
- `observability`

### 2.4 共享基础模块
共享基础模块为多个 owner 提供通用能力。该类模块应优先建设：
- 稳定的小测试
- 契约测试
- 最小共享 fixture

其目标不是承载跨 owner 场景，而是提供可靠基础能力。

---

## 3. Test Type Definitions

### 3.1 `unit`
用于验证：
- 纯逻辑
- 纯算法
- 纯映射
- 小粒度状态逻辑

要求：
- 快
- 稳
- 确定性强
- 尽量不依赖真实网络、真实硬件、真实时间

### 3.2 `contracts`
用于冻结模块对外行为：
- 输入契约
- 输出契约
- 错误契约
- 观测契约

### 3.3 `golden`
用于将输入与期望输出固定为可机器比较的事实样本。常见于：
- 几何产物
- 工艺路径产物
- 载荷组装结果
- 报告结构

### 3.4 `compatibility`
用于验证：
- 老版本与新版本兼容
- schema / payload / serializer 兼容
- 协议对象向前/向后兼容

### 3.5 `integration`
用于验证：
- 跨 owner 装配
- 上下游边界拼接
- 共享样本在多模块间的一致消费

### 3.6 `e2e`
用于验证：
- acceptance 场景
- simulated-line 场景
- HIL 场景

### 3.7 `performance`
用于验证：
- cold / hot / singleflight
- profile 基线
- latency / throughput / memory 变化

### 3.8 `fault-injection`
用于验证：
- 非法输入
- 配置缺失
- 超时
- 乱序响应
- 重复响应
- 中途断连
- 未就绪
- 反馈缺失

### 3.9 `recovery`
用于验证：
- reset
- retry
- rollback
- 重新进入 ready
- 故障后收敛行为

### 3.10 `soak/stress`
用于验证：
- 长时间运行稳定性
- 边界资源压力
- 并发压力
- 状态是否长期可收敛

---

## 4. Sample Type Definitions

### 4.1 `small`
最小可跑通样本，用于快速验证主语义与最小闭环。

### 4.2 `normal`
真实业务常规样本，用于验证标准场景。

### 4.3 `dirty`
脏数据或降级输入样本，用于验证鲁棒性与错误分类。

### 4.4 `boundary`
边界条件样本，用于验证极端数值、极端状态与边缘组合。

### 4.5 `failure`
明确预期失败的样本，用于验证失败口径、阻断口径与证据产出。

---

## 5. Failure Injection Dictionary

### 5.1 `invalid-input`
输入格式、字段、值域非法。

### 5.2 `missing-config`
关键配置、模板、标定参数或依赖项缺失。

### 5.3 `mode-mismatch`
运行模式、宿主模式、mock/real 模式不一致。

### 5.4 `timeout`
请求、事件、状态收敛、反馈等待超时。

### 5.5 `duplicate-response`
同一响应或同一事件被重复注入。

### 5.6 `out-of-order-response`
事件或响应顺序异常。

### 5.7 `disconnect-mid-flight`
执行中途断连、会话断开或 transport 被打断。

### 5.8 `not-ready`
设备或运行时前提未就绪。

### 5.9 `feedback-missing`
命令已下发但关键反馈缺失。

### 5.10 `superseded-artifact`
较新的 artifact / task / plan 覆盖旧产物。

### 5.11 `retry-exhausted`
重试次数耗尽后仍无法恢复。

### 5.12 `rollback-required`
失败后必须显式进入回滚路径。

---

## 6. Module Matrix Overview

### 6.1 总表
| 模块 | 分组 | 优先级 | 最低必备测试类型 |
|---|---|---:|---|
| `job-ingest` | 产物型 | P0 | unit, contracts, golden, integration |
| `dxf-geometry` | 产物型 | P0 | unit, contracts, golden, integration |
| `topology-feature` | 产物型 | P1 | unit, contracts, golden, integration |
| `process-planning` | 产物型 | P1 | unit, contracts, golden |
| `coordinate-alignment` | 产物型 | P1 | unit, contracts, golden |
| `process-path` | 产物型 | P0 | unit, contracts, golden, integration |
| `motion-planning` | 产物型 | P0 | unit, contracts, golden, performance, integration |
| `dispense-packaging` | 产物型 | P0 | unit, contracts, compatibility, golden, integration |
| `runtime-execution` | 运行时控制型 | P0 | unit, contracts, state-machine, fault-injection, recovery |
| `workflow` | 运行时控制型 | P0 | unit, contracts, integration, fault-injection |
| `trace-diagnostics` | 观测与交互型 | P1 | contracts, evidence, observability |
| `hmi-application` | 观测与交互型 | P2 | presenter/view-model, startup-state, protocol |
| `apps/runtime-service` | 运行时控制型 | P0 | unit, contracts, protocol, service-integration |
| `shared/kernel` | 共享基础模块 | P0 | unit, contracts |

### 6.2 P0 模块
P0 模块是第一批必须补齐正式测试面的模块，优先覆盖主风险带：
- 工艺语义到执行载荷传递链
- runtime 状态机与恢复路径
- 宿主装配与模式切换
- 协议兼容与事件时序

### 6.3 P1 模块
P1 模块属于第二批补强对象，主要用于：
- 完善上游链
- 补齐证据体系
- 固化数值/拓扑/对齐等关键中间语义

### 6.4 P2 模块
P2 模块属于第三批补强对象，重点不是数量，而是：
- 观测一致性
- 启动链状态投影
- 用户可见约束正确性

---

## 7. Per-Module Test Requirements

### 7.1 `job-ingest`
#### Inputs And Outputs
输入：作业输入文件、作业元数据。  
输出：可进入几何链的标准 job 入口对象。

#### Required Test Types
- `unit`
- `contracts`
- `golden`
- `integration`

#### Minimum Sample Set
- `small`：最小 DXF / 最小作业定义
- `normal`：常规业务作业
- `dirty`：损坏文件、缺失字段、扩展名不符
- `failure`：重复提交、无效路径、元数据缺失

#### Required Failure Injections
- `invalid-input`
- `missing-config`
- `superseded-artifact`

#### Repository-Level Coverage Mapping
- `tests/integration/scenarios/` 中应包含 `job-ingest -> dxf-geometry` 最小链路

#### Priority
P0

### 7.2 `dxf-geometry`
#### Inputs And Outputs
输入：上游作业输入与 DXF 原始内容。  
输出：标准化几何 primitive / contour / segment 产物。

#### Required Test Types
- `unit`
- `contracts`
- `golden`
- `integration`

#### Minimum Sample Set
- `small`：简单线段/矩形/圆弧
- `normal`：常规业务 DXF
- `dirty`：自交、重叠段、单位异常、退化段
- `boundary`：超短段、极小容差、极大坐标
- `failure`：无法解析或语义无效的样本

#### Required Failure Injections
- `invalid-input`
- `missing-config`

#### Repository-Level Coverage Mapping
- 共享 `samples/dxf` / `samples/golden`
- `tests/integration/scenarios/` 上游链

#### Priority
P0

### 7.3 `topology-feature`
#### Inputs And Outputs
输入：标准化几何。  
输出：拓扑关系、区域/孔洞/岛链等特征产物。

#### Required Test Types
- `unit`
- `contracts`
- `golden`
- `integration`

#### Minimum Sample Set
- `small`：单闭合轮廓
- `normal`：孔洞、嵌套轮廓
- `dirty`：断裂轮廓、方向混乱
- `boundary`：近似接触、多层嵌套
- `failure`：无法确定拓扑关系的样本

#### Required Failure Injections
- `invalid-input`

#### Repository-Level Coverage Mapping
- 可作为第二阶段接入上游链 integration

#### Priority
P1

### 7.4 `process-planning`
#### Inputs And Outputs
输入：几何/拓扑特征与工艺规则。  
输出：工艺规划结果、模板选择结果或规则绑定结果。

#### Required Test Types
- `unit`
- `contracts`
- `golden`

#### Minimum Sample Set
- `small`：最小工艺模板样本
- `normal`：常规工艺模板组合
- `dirty`：模板缺失、配置冲突
- `boundary`：极限参数组合
- `failure`：无法选择工艺模板的样本

#### Required Failure Injections
- `missing-config`
- `mode-mismatch`

#### Repository-Level Coverage Mapping
- 第二阶段接入 `topology-feature -> process-planning -> process-path`

#### Priority
P1

### 7.5 `coordinate-alignment`
#### Inputs And Outputs
输入：规划前几何/工艺对象与标定/坐标参数。  
输出：设备坐标系下的对齐结果。

#### Required Test Types
- `unit`
- `contracts`
- `golden`

#### Minimum Sample Set
- `small`：原点偏移
- `normal`：旋转/镜像/偏移组合
- `boundary`：极限角度、极限偏移、极限标定
- `failure`：非法标定参数

#### Required Failure Injections
- `missing-config`
- `invalid-input`

#### Repository-Level Coverage Mapping
- 第二阶段接入与 `process-path` 前置链路的集成验证

#### Priority
P1

### 7.6 `process-path`
#### Inputs And Outputs
输入：工艺规划结果与对齐/几何对象。  
输出：可进入运动规划的工艺语义路径。

#### Required Test Types
- `unit`
- `contracts`
- `golden`
- `integration`

#### Minimum Sample Set
- `small`：最小可开停胶路径
- `normal`：常规业务路径
- `boundary`：高转角、短段、闭合路径
- `failure`：语义不完整或无法构造工艺路径的样本

#### Required Failure Injections
- `invalid-input`
- `missing-config`

#### Repository-Level Coverage Mapping
- `tests/integration/scenarios/` 中应含 `process-path -> motion-planning`

#### Priority
P0

### 7.7 `motion-planning`
#### Inputs And Outputs
输入：工艺语义路径。  
输出：时间律与运动规划结果。

#### Required Test Types
- `unit`
- `contracts`
- `golden`
- `performance`
- `integration`

#### Minimum Sample Set
- `small`：最小可规划路径
- `normal`：常规规划样本
- `boundary`：极短段、极限加速度/jerk 约束
- `failure`：不可规划样本
- `medium/large`：用于 performance profile

#### Required Failure Injections
- `invalid-input`
- `timeout`（如存在规划时限）
- `superseded-artifact`

#### Repository-Level Coverage Mapping
- `tests/integration/scenarios/`
- `tests/performance/profiles/`

#### Priority
P0

### 7.8 `dispense-packaging`
#### Inputs And Outputs
输入：运动规划与工艺语义对象。  
输出：下游执行载荷。

#### Required Test Types
- `unit`
- `contracts`
- `compatibility`
- `golden`
- `integration`

#### Minimum Sample Set
- `small`：最小 payload
- `normal`：常规 payload
- `boundary`：极限段数、极限字段组合
- `failure`：必填字段缺失、版本不兼容样本

#### Required Failure Injections
- `invalid-input`
- `missing-config`
- `mode-mismatch`

#### Repository-Level Coverage Mapping
- `tests/integration/protocol-compatibility/`
- `tests/integration/scenarios/`

#### Priority
P0

### 7.9 `runtime-execution`
#### Inputs And Outputs
输入：执行载荷、宿主命令、协议事件、反馈事件。  
输出：执行状态、故障状态、恢复状态。

#### Required Test Types
- `unit`
- `contracts`
- `state-machine`
- `fault-injection`
- `recovery`
- 必要时 `integration`

#### Minimum Sample Set
- `small`：最小启动/停止
- `normal`：执行、暂停、恢复、reset
- `boundary`：高频事件、重复命令、快速 stop/reset
- `failure`：故障后恢复失败、反馈缺失、状态卡死

#### Required Failure Injections
- `timeout`
- `duplicate-response`
- `out-of-order-response`
- `disconnect-mid-flight`
- `not-ready`
- `feedback-missing`
- `retry-exhausted`
- `rollback-required`

#### Repository-Level Coverage Mapping
- simulated-line acceptance
- `tests/integration/scenarios/` runtime 链

#### Priority
P0

### 7.10 `workflow`
#### Inputs And Outputs
输入：artifact / task / stage 事件。  
输出：工作流阶段状态、生命周期状态、收敛状态。

#### Required Test Types
- `unit`
- `contracts`
- `integration`
- `fault-injection`

#### Minimum Sample Set
- `small`：单任务单阶段
- `normal`：多阶段顺序推进
- `boundary`：并发推进、重复推进
- `failure`：timeout、rollback、superseded 场景

#### Required Failure Injections
- `timeout`
- `duplicate-response`
- `superseded-artifact`
- `rollback-required`
- `retry-exhausted`

#### Repository-Level Coverage Mapping
- `tests/integration/scenarios/`
- simulated-line acceptance

#### Priority
P0

### 7.11 `trace-diagnostics`
#### Inputs And Outputs
输入：事件、状态、报告、故障归因线索。  
输出：证据结构、timeline、分类结果。

#### Required Test Types
- `contracts`
- `evidence`
- `observability`

#### Minimum Sample Set
- `normal`：成功场景证据
- `failure`：失败场景证据
- `boundary`：事件缺失、顺序异常、信息不全

#### Required Failure Injections
- `missing-config`
- `invalid-input`
- `out-of-order-response`

#### Repository-Level Coverage Mapping
- 与 `04-baseline-fixture-golden-evidence.md` 对齐

#### Priority
P1

### 7.12 `hmi-application`
#### Inputs And Outputs
输入：runtime 状态、宿主状态、用户操作事件。  
输出：界面状态投影、交互可用性、用户可见提示。

#### Required Test Types
- `presenter/view-model`
- `startup-state`
- `protocol`
- 必要时少量 `integration`

#### Minimum Sample Set
- `small`：offline / connecting / ready
- `normal`：executing / paused / recovery
- `failure`：fault / not-ready / startup failure

#### Required Failure Injections
- `not-ready`
- `timeout`
- `mode-mismatch`
- `disconnect-mid-flight`

#### Repository-Level Coverage Mapping
- 第二阶段以后再补强，优先状态投影而非 GUI 自动化数量

#### Priority
P2

### 7.13 `apps/runtime-service`
#### Inputs And Outputs
输入：配置、模式选择、宿主入口、外部调用。  
输出：服务启动状态、协议面、装配后的运行环境。

#### Required Test Types
- `unit`
- `contracts`
- `protocol`
- `service-integration`
- `bootstrap-smoke`

#### Minimum Sample Set
- `small`：最小 mock 配置
- `normal`：常规 real/mock 配置
- `dirty`：配置缺失、冲突、路径错误
- `failure`：bootstrap 假成功、模式切换失败

#### Required Failure Injections
- `missing-config`
- `mode-mismatch`
- `timeout`
- `not-ready`

#### Repository-Level Coverage Mapping
- `tests/integration/scenarios/` runtime 链
- simulated-line 主环境装配入口

#### Priority
P0

### 7.14 `shared/kernel`
#### Inputs And Outputs
输入：共享基础对象、共享工具逻辑。  
输出：共享基础能力。

#### Required Test Types
- `unit`
- `contracts`

#### Minimum Sample Set
- `small`：最小共享逻辑样本
- `boundary`：基础边界值样本
- `failure`：非法值或无效对象样本

#### Required Failure Injections
- `invalid-input`

#### Repository-Level Coverage Mapping
- 根级 canonical root 聚合

#### Priority
P0

---

## 8. Priority And Build Order

### 8.1 第一批模块
第一批必须优先建设：
1. `runtime-execution`
2. `motion-planning`
3. `process-path`
4. `dispense-packaging`
5. `apps/runtime-service`
6. `workflow`
7. `dxf-geometry`
8. `job-ingest`

### 8.2 第二批模块
第二批用于补强中间语义与证据体系：
- `topology-feature`
- `process-planning`
- `coordinate-alignment`
- `trace-diagnostics`

### 8.3 第三批模块
第三批用于补强观测与交互层：
- `hmi-application`

### 8.4 暂缓模块
任何未进入当前主风险带、且缺少清晰 owner / 清晰契约面的模块，可在后续阶段再纳入正式矩阵。

---

## 9. Coverage Expectations

### 9.1 每类模块的最低覆盖要求
- 产物型模块：至少具备 `unit + contracts + golden`
- 运行时控制型模块：至少具备 `unit + contracts + fault-injection`
- 观测与交互型模块：至少具备 `contracts + 状态投影测试`

### 9.2 不同优先级模块的最低测试面
- P0：必须进入正式根级调度或正式模块内 tests
- P1：至少在模块内建立正式目录与最小样本
- P2：至少冻结测试面与最小状态集合

### 9.3 覆盖不足的判定条件
满足以下任一条件，可判定为覆盖不足：
- 只有 E2E，没有模块内 contracts
- 只有日志断言，没有 golden / 结构化证据
- 只有成功路径，没有 failure / recovery
- 只有 small，没有 normal 或 boundary

---

## 10. Normative Rules

### 10.1 MUST
- 每个正式模块必须有明确测试类型矩阵。
- 每个 P0 模块必须具备正式模块内测试入口。
- 运行时控制型模块必须具备故障注入测试。
- 产物型模块必须具备至少一类 golden 测试。

### 10.2 SHOULD
- 每个模块都应至少拥有一组 `failure` 样本。
- 第二阶段应补齐 P1 模块的正式矩阵。
- HMI 类模块应优先测试状态投影，而不是首先堆 GUI 自动化。

### 10.3 MUST NOT
- 不得仅以仓库级 E2E 代替模块矩阵。
- 不得以“已有人工验证”替代正式 failure injection。
- 不得让模块矩阵长期停留在口头约定而无正式文档。
