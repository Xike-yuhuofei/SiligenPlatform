# 冗余排查治理 Day 1：实体盘点与边界冻结（process-runtime-core）

## 文档元信息

- 版本：`v0.1-day1`
- 日期：`2026-03-21`
- 负责人：`Codex`
- 范围：`packages/process-runtime-core`
- 里程碑：`数据模型 + 接口契约冻结`（本文件覆盖 Day 1 输入冻结）

## 1. Day 1 冻结结论

1. `packages/process-runtime-core` 是本次冗余排查治理在业务内核侧的唯一 canonical owner。
2. 扫描对象限定为 package 内源码与测试，不采集 `runtime-host`、`transport-gateway`、`device-adapters` 的实现细节。
3. 实体模型按“双证据闭环”最小集合冻结为：`CodeEntity`、`StaticEvidence`、`RuntimeEvidence`、`Candidate`、`DecisionLog`。
4. 逻辑边界遵循现有 package 文档（`BOUNDARIES.md`、`README.md`）并与 CMake target 子域一致。

## 2. 领域边界与上下文映射

| 上下文 | 代码主路径 | 说明 |
|---|---|---|
| application | `src/application/**` | 用例编排与服务入口，不承接宿主/传输实现 |
| domain.process/planning/execution | `modules/process-core/**`、`modules/motion-core/**`、`src/domain/{configuration,trajectory,motion,dispensing}/**` | 核心业务规则与规划执行能力 |
| domain.machine | `src/domain/machine/**` | 设备能力抽象、校准与模型规则 |
| domain.safety | `src/domain/safety/**` | 联锁与软限位等安全规则 |
| domain.recipes | `src/domain/recipes/**`、`src/application/usecases/recipes/**` | 配方模型、校验与版本操作 |
| domain.diagnostics | `src/domain/diagnostics/**` | 诊断记录、过程结果序列化 |
| tests | `tests/**` | 单测与集成测试证据来源 |

## 3. Day 1 实体盘点（数据模型草案输入）

### 3.1 CodeEntity（代码实体）

- 目标：统一标识可被判定是否冗余的最小单元。
- 粒度：`function`、`method`、`class`、`file`、`cmake_target`。
- 建议字段：

| 字段 | 类型 | 约束 | 说明 |
|---|---|---|---|
| `entity_id` | string | 必填，全局唯一 | 建议由 `path + symbol + signature hash` 生成 |
| `entity_type` | enum | 必填 | `function/method/class/file/cmake_target` |
| `symbol` | string | 可空 | 文件级实体可空 |
| `path` | string | 必填 | 相对仓库路径 |
| `module` | string | 必填 | 子域模块标识 |
| `owner_target` | string | 可空 | 对应 CMake target |
| `language` | enum | 必填 | 当前先支持 `cpp/header/cmake` |
| `last_modified_at` | datetime | 可空 | 来自 git 变更信息 |

### 3.2 StaticEvidence（静态证据）

- 目标：记录“未引用/不可达/重复实现”类证据。
- 建议字段：

| 字段 | 类型 | 约束 | 说明 |
|---|---|---|---|
| `evidence_id` | string | 必填 | 唯一标识 |
| `entity_id` | string | 必填 | 关联 `CodeEntity` |
| `rule_id` | string | 必填 | 静态规则标识 |
| `rule_version` | string | 必填 | 规则版本 |
| `signal_type` | enum | 必填 | `unreferenced/unreachable/duplicate/isolated` |
| `signal_strength` | number | 必填 | `0~1` |
| `details` | json | 必填 | 引用关系、命中路径、重复簇 |
| `collected_at` | datetime | 必填 | 采集时间 |

### 3.3 RuntimeEvidence（动态证据）

- 目标：记录真实运行命中信息，降低误删。
- 建议字段：

| 字段 | 类型 | 约束 | 说明 |
|---|---|---|---|
| `evidence_id` | string | 必填 | 唯一标识 |
| `entity_id` | string | 必填 | 关联 `CodeEntity` |
| `environment` | enum | 必填 | `test/staging/canary` |
| `window_start` | datetime | 必填 | 统计窗口开始 |
| `window_end` | datetime | 必填 | 统计窗口结束 |
| `hit_count` | integer | 必填 | 命中次数 |
| `last_hit_at` | datetime | 可空 | 无命中时为空 |
| `source` | string | 必填 | 测试任务或运行链路标识 |
| `collected_at` | datetime | 必填 | 采集时间 |

### 3.4 Candidate（冗余候选）

- 目标：融合证据并形成处置队列。
- 建议字段：

| 字段 | 类型 | 约束 | 说明 |
|---|---|---|---|
| `candidate_id` | string | 必填 | 唯一标识 |
| `entity_id` | string | 必填 | 关联 `CodeEntity` |
| `redundancy_score` | number | 必填 | `R`，`0~100` |
| `deletion_risk_score` | number | 必填 | `D`，`0~100` |
| `priority` | enum | 必填 | `P0/P1/P2/P3` |
| `recommended_action` | enum | 必填 | `observe/deprecate/remove/hold` |
| `status` | enum | 必填 | `NEW/REVIEWED/DEPRECATED/OBSERVED/REMOVED/ROLLED_BACK` |
| `computed_at` | datetime | 必填 | 评分时间 |

### 3.5 DecisionLog（决策日志）

- 目标：审计评审、删除与回滚。
- 建议字段：

| 字段 | 类型 | 约束 | 说明 |
|---|---|---|---|
| `decision_id` | string | 必填 | 唯一标识 |
| `candidate_id` | string | 必填 | 关联 `Candidate` |
| `action` | enum | 必填 | `approve/reject/deprecate/remove/rollback` |
| `actor` | string | 必填 | 操作者 |
| `reason` | string | 必填 | 决策理由 |
| `evidence_snapshot` | json | 必填 | 决策时证据快照 |
| `ticket` | string | 可空 | 任务号 |
| `created_at` | datetime | 必填 | 创建时间 |

## 4. 采集边界冻结（Day 1）

1. 允许采集：`packages/process-runtime-core/src/**`、`packages/process-runtime-core/tests/**`、`packages/process-runtime-core/CMakeLists.txt`。
2. 允许读取但不纳入冗余评分：`packages/process-runtime-core/BOUNDARIES.md`、`README.md`。
3. 禁止纳入本阶段评分：`packages/runtime-host/**`、`packages/transport-gateway/**`、`packages/device-adapters/**`。
4. 禁止新增跨边界直连依赖作为“修复冗余”的手段。

## 5. Day 2 输入清单（从本冻结推进）

1. 将本文件的 5 类实体转换为正式 schema（字段类型、主键、索引、枚举定义）。
2. 为 `signal_type`、`status`、`recommended_action` 输出统一枚举字典。
3. 补充 `entity_id` 生成策略的确定性规范（同名重载、模板实例、匿名命名空间处理）。

## 6. 验收标准（Day 1）

1. 已给出可执行的实体定义，不再口头新增实体类别。
2. 已明确采集范围与禁入范围，避免后续边界漂移。
3. 已与 package 既有边界文档一致，无冲突项。
