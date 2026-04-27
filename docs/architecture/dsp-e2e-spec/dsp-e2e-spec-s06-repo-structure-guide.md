# 《点胶机端到端流程规格说明 s06：仓库目录结构建议（修订版）》

Status: Frozen
Authority: Primary Spec Axis S06
Applies To: repository structure and dependency placement
Owns / Covers: apps/modules/shared/tests/docs/config/data/scripts placement, dependency direction, test location
Must Not Override: S04 object schema; S05 module owner; repository AGENTS.md canonical roots
Read When: placing code, tests, docs, contracts, fixtures, or shared utilities
Conflict Priority: use S06 for repo placement; defer business ownership to S05 and stable contract shape to S04
Codex Keywords: repo structure, apps, modules, shared, tests, scripts, dependency direction, single repo multi module

---

## Codex Decision Summary

- 本文裁决正式代码、契约、测试、文档与脚本在仓库中的落位方式。
- 本文不裁决业务 owner 或对象字段；owner 以 S05 为准，对象字段以 S04 为准。
- 不得把业务语义放入 `shared/`；不得重引入 retired roots；adapter 与 app 入口不得成为长期业务事实源。

---
本版目标是把前面的**阶段、对象、模块边界、命令/事件、状态机、测试矩阵**真正压到可开工的工程组织形式上。

重点不是“代码怎么写”，而是：

- 仓库顶层怎么分
- 模块目录怎么落
- `contracts / domain / services / application / adapters / runtime` 分别放哪
- 哪些对象与消息要进入公共层
- 哪些依赖允许，哪些依赖必须禁止
- 测试、金样本、文档、时序图放哪
- 如何保证 `artifact_ref / command / event / failure_payload` 全链统一

本版默认采用：**单仓多模块**。

---

# 1. 仓库总体定位

## 1.1 建议形态

建议采用：

**一个主仓库 + 多个一级模块包 + 少量应用入口 + 统一契约层**

也就是：
- 一个仓库承载完整产品
- 规划、执行、追溯、HMI 等按模块拆分
- 共享契约、公共对象、错误码、ID 与消息 envelope 进入稳定公共层
- 应用入口与领域模块分离

## 1.2 为什么当前阶段不建议过早多仓

不是边界不清，而是当前链路从 DXF 到设备执行高度联调，过早分仓会放大：
- 版本协调成本
- 接口演进成本
- 多仓发布与回滚成本
- 联调环境与样本一致性管理成本

因此第一阶段建议：**单仓多模块 + 契约优先 + 事件优先**。

---

# 2. 顶层目录建议

```text
repo/
├─ apps/
│  ├─ hmi-app/
│  ├─ planner-cli/
│  ├─ runtime-service/
│  ├─ trace-viewer/
│  └─ tooling/
├─ modules/
│  ├─ workflow/
│  ├─ job-ingest/
│  ├─ dxf-geometry/
│  ├─ topology-feature/
│  ├─ process-planning/
│  ├─ coordinate-alignment/
│  ├─ process-path/
│  ├─ motion-planning/
│  ├─ dispense-packaging/
│  ├─ runtime-execution/
│  ├─ trace-diagnostics/
│  └─ hmi-application/
├─ shared/
│  ├─ kernel/
│  ├─ ids/
│  ├─ artifacts/
│  ├─ contracts/
│  ├─ commands/
│  ├─ events/
│  ├─ failures/
│  ├─ messaging/
│  ├─ logging/
│  ├─ config/
│  └─ testing/
├─ docs/
│  ├─ architecture/
│  ├─ specifications/
│  ├─ workflows/
│  ├─ runbooks/
│  └─ adr/
├─ samples/
│  ├─ dxf/
│  ├─ jobs/
│  ├─ fixtures/
│  ├─ process-templates/
│  └─ golden-cases/
├─ tests/
│  ├─ integration/
│  ├─ e2e/
│  ├─ performance/
│  └─ contract-lint/
├─ scripts/
├─ build/
├─ deploy/
├─ third_party/
├─ .github/
└─ README.md
```

---

# 3. 顶层目录职责

## 3.1 `apps/`
放**应用入口**，不放核心领域逻辑。

适合放：
- `hmi-app/`：桌面或 Web HMI 启动入口
- `planner-cli/`：离线规划、批量校验、调试命令行
- `runtime-service/`：设备执行服务入口
- `trace-viewer/`：追溯查看与导出工具
- `tooling/`：数据导入导出、样本转换、离线诊断工具

不适合放：
- DXF 解析核心
- 工艺规划核心
- 运动规划核心
- owner 对象定义

原则：  
**apps 负责组装与发布，不负责发明领域事实。**

---

## 3.2 `modules/`
放一级业务模块，是主战场。

每个模块对应唯一职责边界，对应 M0-M11：
- `workflow`
- `job-ingest`
- `dxf-geometry`
- `topology-feature`
- `process-planning`
- `coordinate-alignment`
- `process-path`
- `motion-planning`
- `dispense-packaging`
- `runtime-execution`
- `trace-diagnostics`
- `hmi-application`

---

## 3.3 `shared/`
放跨模块共享但**不承载具体业务决策**的公共层。

只允许放：
- 基础类型
- 统一 ID / 引用 / 状态
- 跨模块 contracts
- command/event envelope
- failure payload
- 统一日志与配置
- 通用测试支撑

不允许放：
- 模糊 owner 的业务逻辑
- “万能工具类”
- 某个模块的半成品规则
- UI 或 runtime 私货

原则：  
**shared 是最容易腐化的地方，必须严格防守。**

---

## 3.4 `docs/`
放正式文档，不放散落笔记。

建议至少分：
- `architecture/`：模块边界、依赖规则、仓库约束
- `specifications/`：s01-s09 这套正式规格
- `workflows/`：端到端流程、回退流程、故障分层、时序图
- `runbooks/`：运维处置、首件不通过、堵针、对位失败等操作手册
- `adr/`：架构决策记录

---

## 3.5 `samples/`
放稳定样本，不放临时垃圾。

建议分：
- `dxf/`：标准 DXF 样例
- `jobs/`：任务上下文样例
- `fixtures/`：治具/坐标/标定样例
- `process-templates/`：工艺模板与固定参数样例
- `golden-cases/`：金标准回归用例

---

## 3.6 `tests/`
放仓库级跨模块测试，模块自己的单测跟模块走。

这里主要放：
- `integration/`：模块对模块
- `e2e/`：整链路
- `performance/`：解析、规划、执行链性能
- `contract-lint/`：contract/schema/事件命名一致性校验

---

# 4. `shared/` 的推荐结构

为了让 s04/s05/s07 的统一消息与对象口径真正落地，建议 `shared/` 至少固定如下：

```text
shared/
├─ kernel/
│  ├─ value-objects/
│  ├─ base-entities/
│  ├─ result/
│  └─ validation/
├─ ids/
│  ├─ job-id/
│  ├─ workflow-id/
│  ├─ request-id/
│  ├─ command-id/
│  ├─ event-id/
│  ├─ machine-id/
│  └─ execution-id/
├─ artifacts/
│  ├─ artifact-ref/
│  ├─ artifact-status/
│  ├─ artifact-version/
│  └─ stage-id/
├─ contracts/
│  ├─ artifacts/
│  ├─ queries/
│  └─ approvals/
├─ commands/
│  ├─ envelopes/
│  │  ├─ command-envelope/
│  │  └─ execution-context/
│  ├─ workflow/
│  ├─ planning/
│  ├─ runtime/
│  └─ trace/
├─ events/
│  ├─ envelopes/
│  │  ├─ event-envelope/
│  │  └─ event-status/
│  ├─ planning/
│  ├─ runtime/
│  ├─ rollback/
│  └─ trace/
├─ failures/
│  ├─ failure-code/
│  ├─ failure-payload/
│  ├─ failure-severity/
│  ├─ rollback-target/
│  └─ recommended-action/
├─ messaging/
│  ├─ idempotency/
│  ├─ correlation/
│  ├─ causation/
│  ├─ event-store/
│  └─ outbox/
├─ logging/
├─ config/
└─ testing/
```

## 4.1 公共层中必须固化的关键契约

### A. 对象引用
统一一个 `artifact_ref` 结构：
- `artifact_type`
- `artifact_id`
- `artifact_version`

### B. Command envelope
至少统一：
- `message_id`
- `command_id`
- `request_id`
- `idempotency_key`
- `job_id`
- `workflow_id`
- `stage_id`
- `artifact_ref`
- `correlation_id`
- `causation_id`
- `execution_context`
- `issued_by`
- `issued_at`

### C. Event envelope
至少统一：
- `message_id`
- `event_id`
- `request_id`
- `job_id`
- `workflow_id`
- `stage_id`
- `artifact_ref`
- `source_command_id`
- `correlation_id`
- `causation_id`
- `execution_context`
- `producer`
- `occurred_at`
- `status`

### D. Failure payload
至少统一：
- `failure_code`
- `severity`
- `retryable`
- `manual_override_allowed`
- `rollback_target`
- `requires_new_job_or_version_chain`
- `blocking_scope`
- `requires_manual_decision`
- `recommended_action`

## 4.2 `shared/` 中禁止出现的内容
禁止放：
- `geometry_helper` 这类几何 owner 不明确的业务逻辑
- `process_utils` 这类看似通用、实则工艺专属的代码
- `runtime_common` 这类执行层私货
- `ui_helpers` 混进核心公共层

一句话：  
**shared 只放真正跨模块、低业务含义、稳定的基础设施内容。**

---

# 5. `modules/` 下的标准目录模板

建议每个一级模块尽量使用统一骨架。

```text
modules/<module-name>/
├─ README.md
├─ module.yaml
├─ contracts/
├─ domain/
├─ services/
├─ application/
├─ adapters/
├─ runtime/        # 可选，仅对运行态模块开放
├─ tests/
│  ├─ unit/
│  ├─ contract/
│  ├─ integration/
│  └─ regression/
└─ examples/
```

---

# 6. 各层职责定义

## 6.1 `contracts/`
定义模块对外暴露的契约。

适合放：
- Command / Query DTO
- Result DTO
- 对外 API / Facade 接口
- 模块事件定义
- schema 约束

不适合放：
- 具体算法实现
- 私有领域规则
- 第三方 SDK 适配代码

---

## 6.2 `domain/`
定义模块拥有的领域对象、值对象、不变量和规则。

适合放：
- owner artifact 模型
- 值对象 / 枚举
- 领域规则
- 状态机核心对象
- 领域校验器

不适合放：
- UI 逻辑
- 数据库存取
- 设备/网络适配器

---

## 6.3 `services/`
放模块核心用例实现，也就是“如何从输入产生产物”。

适合放：
- `BuildProcessPathService`
- `PlanMotionService`
- `AssembleExecutionPackageService`
- `FinalizeExecutionRecordService`

不适合放：
- 领域模型本体
- 第三方驱动
- UI 控件逻辑

说明：  
`services/` 不是“所有逻辑的垃圾桶”，它只放本模块的用例编排。

---

## 6.4 `application/`
把本模块包装成可被外部调用的应用层入口。

适合放：
- command handlers
- query handlers
- transaction orchestration
- 审批前置检查
- facade

---

## 6.5 `adapters/`
放外部依赖适配层。

适合放：
- DXF 库适配
- 文件存储适配
- 数据库 / 事件存储适配
- 视觉 SDK 适配
- 控制卡 / PLC / IO 适配
- 报警网关 / tracing / export 适配

原则：  
**外部世界只能通过 `adapters/` 进入系统，不得侵入 domain/services。**

---

## 6.6 `runtime/`
只给**运行态模块**或确实有实时状态机的模块使用。

适合放：
- 执行状态机
- 会话管理器
- 监控器
- 检查点恢复器
- 故障分层器
- 持续采样/订阅器

说明：
- `dxf-geometry` 通常不需要 `runtime/`
- `workflow` 可有轻量状态机实现
- `runtime-execution` 一定需要

---

## 6.7 `tests/`
模块内测试跟着模块走。

建议固定为：
- `unit/`
- `contract/`
- `integration/`
- `regression/`

其中：
- `unit/`：单元测试
- `contract/`：接口契约/消息 schema 测试
- `integration/`：模块对外部 adapter 的集成测试
- `regression/`：金样本回归

---

## 6.8 `examples/`
放最小样例和调试输入。

非常适合：
- 解析样例
- 工艺规划样例
- 轨迹生成样例
- 时序生成样例
- 预检阻断与首件失败样例

---

## 6.9 `module.yaml`
作为模块元信息清单。

建议至少写：
- 模块名称
- owner
- owned_artifacts
- consumed_commands
- emitted_events
- upstream_contracts
- forbidden_dependencies
- test_targets
- sample_refs

---

# 7. M0-M11 的目录落位建议

| 模块 | 目录 | owner 对象 / 核心事实 |
|---|---|---|
| M0 | `modules/workflow/` | `WorkflowRun`、`StageTransitionRecord`、`RollbackDecision` |
| M1 | `modules/job-ingest/` | `JobDefinition`、`SourceDrawing` |
| M2 | `modules/dxf-geometry/` | `CanonicalGeometry` |
| M3 | `modules/topology-feature/` | `TopologyModel`、`FeatureGraph` |
| M4 | `modules/process-planning/` | `ProcessPlan` |
| M5 | `modules/coordinate-alignment/` | `CoordinateTransformSet`、`AlignmentCompensation` |
| M6 | `modules/process-path/` | `ProcessPath` |
| M7 | `modules/motion-planning/` | `MotionPlan` |
| M8 | `modules/dispense-packaging/` | `DispenseTimingPlan`、`ExecutionPackage` |
| M9 | `modules/runtime-execution/` | `ExecutionSession`、`MachineReadySnapshot`、`FirstArticleResult`、运行时状态流 |
| M10 | `modules/trace-diagnostics/` | `ExecutionRecord`、`TraceLinkSet` |
| M11 | `modules/hmi-application/` | `UserCommand`、`ApprovalDecision`、`UIViewModel` |

---

# 8. 依赖规则建议

这是整个仓库最重要的约束之一。

## 8.1 允许的依赖方向
只允许：
- `apps -> modules -> shared`
- `modules -> shared`
- 少数模块通过 **对方的 contracts** 单向依赖
- `runtime-execution` 依赖上游对象的只读 contracts，不依赖上游实现
- `trace-diagnostics` 依赖各模块 events/contracts，不依赖 domain 私有实现

## 8.2 明确禁止的依赖方向
禁止：
- `shared -> modules`
- `process-planning -> runtime-execution` 直接依赖运行时实现
- `hmi-application -> runtime-execution/adapters` 直接控设备
- `dispense-packaging -> process-path/services` 反向重排路径
- `trace-diagnostics -> 任意模块/domain` 反向修正事实
- `apps -> modules/*/adapters` 绕过 contracts/application

## 8.3 推荐依赖矩阵

| 模块 | 可依赖 |
|---|---|
| `workflow` | 各模块 `contracts`、`shared` |
| `job-ingest` | `shared` |
| `dxf-geometry` | `job-ingest/contracts`、`shared` |
| `topology-feature` | `dxf-geometry/contracts`、`shared` |
| `process-planning` | `topology-feature/contracts`、`shared` |
| `coordinate-alignment` | `process-planning/contracts`、`shared` |
| `process-path` | `process-planning/contracts`、`coordinate-alignment/contracts`、`shared` |
| `motion-planning` | `process-path/contracts`、`shared` |
| `dispense-packaging` | `motion-planning/contracts`、`process-planning/contracts`、`shared` |
| `runtime-execution` | `dispense-packaging/contracts`、`shared` |
| `trace-diagnostics` | 各模块 `contracts`、各类 `events`、`shared` |
| `hmi-application` | `workflow/contracts`、`job-ingest/contracts`、`runtime-execution/contracts`、查询契约、`shared` |

---

# 9. 契约优先通信方式

## 9.1 同进程场景
优先通过：
- `contracts` 接口
- `application facade`
- `command/query handler`

## 9.2 跨进程场景
优先通过：
- Command 消息
- Query API
- Event 流

## 9.3 禁止方式
- 直接 import 对方 `services/`
- 直接 import 对方 `adapters/`
- 直接改写对方 `domain/` 对象内部字段

---

# 10. 测试与金样本目录落位规则

## 10.1 模块内测试跟模块走
例如：
- `modules/dxf-geometry/tests/unit/`
- `modules/motion-planning/tests/regression/`
- `modules/runtime-execution/tests/integration/`

## 10.2 仓库级测试只放跨模块链路
例如：

```text
tests/integration/
├─ source-to-geometry/
├─ feature-to-process/
├─ process-to-motion/
├─ motion-to-package/
└─ package-to-runtime/
```

```text
tests/e2e/
├─ full-success-chain/
├─ first-article-reject-rollback/
├─ preflight-blocked/
├─ runtime-fault-recovery/
└─ archive-trace-chain/
```

## 10.3 金样本单独维护
建议：

```text
samples/golden-cases/
├─ simple-closed-bead/
├─ open-path-with-travel/
├─ mirrored-part-case/
├─ alignment-offset-case/
├─ alignment-not-required-case/
├─ first-article-reject-case/
├─ preflight-blocked-case/
├─ runtime-fault-recovery-case/
└─ archive-trace-case/
```

每个金样本下至少保存：
- `input/`
- `expected/artifacts/`
- `expected/events/`
- `expected/states/`
- `metadata/`

---

# 11. 文档与代码的映射建议

建议 `docs/architecture/modules/` 与 `modules/` 一一对齐：

```text
docs/architecture/modules/
├─ workflow.md
├─ job-ingest.md
├─ dxf-geometry.md
├─ topology-feature.md
├─ process-planning.md
├─ coordinate-alignment.md
├─ process-path.md
├─ motion-planning.md
├─ dispense-packaging.md
├─ runtime-execution.md
├─ trace-diagnostics.md
└─ hmi-application.md
```

每份模块文档固定写 8 件事：
1. 模块职责
2. owner 对象
3. 输入契约
4. 输出契约
5. emitted events
6. 禁止越权边界
7. 测试入口
8. 金样本入口

---

# 12. 最小工程启动骨架建议

如果现在就开始搭项目骨架，第一批建议先落下面这些目录：

```text
repo/
├─ apps/
│  ├─ hmi-app/
│  ├─ planner-cli/
│  └─ runtime-service/
├─ modules/
│  ├─ workflow/
│  ├─ job-ingest/
│  ├─ dxf-geometry/
│  ├─ topology-feature/
│  ├─ process-planning/
│  ├─ coordinate-alignment/
│  ├─ process-path/
│  ├─ motion-planning/
│  ├─ dispense-packaging/
│  ├─ runtime-execution/
│  └─ trace-diagnostics/
├─ shared/
│  ├─ ids/
│  ├─ artifacts/
│  ├─ contracts/
│  ├─ commands/
│  ├─ events/
│  ├─ failures/
│  └─ testing/
├─ docs/
│  ├─ architecture/
│  └─ specifications/
├─ samples/
│  ├─ dxf/
│  └─ golden-cases/
└─ tests/
   ├─ integration/
   └─ e2e/
```

这是一个足够稳、又不会太重的起步骨架。

---

# 13. 这一版最关键的工程结论

## 13.1 目录结构不是审美问题
它本质上是在表达：
- 责任边界
- 依赖方向
- 事实 owner
- 控制与运行边界

## 13.2 最值得防守的不是“模块数量太多”
而是：
- `shared` 腐化
- `apps` 吞业务
- `runtime` 越权改规划
- `HMI` 直控设备
- `trace` 反向修正事实

## 13.3 这套单仓多模块骨架的核心是三条线

### 规划线
`job-ingest -> dxf-geometry -> topology-feature -> process-planning -> coordinate-alignment -> process-path -> motion-planning -> dispense-packaging`

### 执行线
`dispense-packaging -> runtime-execution`

### 追溯线
`所有模块 -> trace-diagnostics`

## 13.4 真正必须统一到公共层的只有 4 类东西
- `artifact_ref`
- command envelope
- event envelope
- failure payload

其余业务规则，应尽量回到 owner 模块，不要污染公共层。
