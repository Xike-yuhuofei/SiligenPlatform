# 10 Roadmap Test Implementation

## 1. Purpose

### 1.1 文档目的
本文档将分层测试体系基准拆解为分阶段实施路线图，用于统一：
- 建设顺序
- 阶段目标
- 交付物
- 验收标准
- 风险与依赖

### 1.2 与 baseline 文档的关系
本路线图建立在以下基准文档之上：
- `01-baseline-architecture.md`
- `02-baseline-directory-and-ownership.md`
- `03-baseline-module-test-matrix.md`
- `04-baseline-fixture-golden-evidence.md`
- `05-baseline-suite-label-ci.md`
- `06-baseline-simulated-line-and-hil.md`
- `08-baseline-performance-and-soak.md`
- `09-baseline-test-data-lifecycle.md`

本路线图不重新定义基准，而是把基准转译为阶段性交付计划。

---

## 2. Current State Assessment

### 2.1 当前已有入口
当前工作区已经具备：
- 根级 `build.ps1`
- 根级 `test.ps1`
- 根级 `ci.ps1`
- 根级 `tests/` 承载面
- 模块内与宿主侧部分正式测试根

### 2.2 当前已有承载面
当前已具备的正式承载面包括：
- `tests/integration`
- `tests/e2e`
- `tests/performance`
- 若干模块内 `tests/`
- `apps/runtime-service/tests`

### 2.3 当前主要短板
当前主要短板不是“完全没有测试”，而是：
- 模块契约测试厚度不足
- golden / fixture / evidence 规范尚未完全冻结
- simulated-line 尚未成为自动化主战场
- HIL 易被误用为主验证面
- performance / soak 缺少统一 baseline 治理

### 2.4 当前优先风险
优先风险集中在：
- 工艺语义 -> 规划 -> 打包 -> 运行时主链
- 状态机与恢复路径
- 协议兼容性
- 宿主启动与模式切换
- 故障注入与可诊断性

---

## 3. Delivery Strategy

### 3.1 骨架优先
先冻结骨架，再扩充内容。骨架包括：
- 文档基准
- 目录与 owner
- suite / label / CI profile
- simulated-line / HIL 边界

### 3.2 判定物优先
优先建设可机器判定的输出，而不是先扩大量手工解释型测试。

判定物优先级：
- contracts
- golden
- evidence bundle
- baseline diff

### 3.3 模拟优先
对高层验证面，应优先建设 simulated-line，而不是优先扩大 HIL 覆盖范围。

### 3.4 HIL 收敛
HIL 仅用于验证 simulated-line 不能证明的真实硬件风险，不得成为默认补救手段。

---

## 4. Phase Plan

### 4.1 Phase 1：冻结骨架
目标：
- 完成测试平台基准文档
- 冻结目录与 owner 规则
- 冻结 suite / label / CI profile
- 冻结 simulated-line / HIL 边界

关键交付物：
- `01 / 02 / 05 / 06` 文档
- 根级执行 profile 与报告口径说明

### 4.2 Phase 2：补 golden 与契约
目标：
- 建立主风险链模块的 contracts / golden / fixture 基准
- 形成模块级最小测试面
- 冻结 evidence bundle

关键交付物：
- `03 / 04` 文档
- P0 模块 contracts / golden 初版
- 正式 fixture / golden 清单

### 4.3 Phase 3：补 fault injection 与 simulated-line
目标：
- 把主要故障场景从人工联机验证前移到自动化
- 建立五类 acceptance 的 simulated-line 主场景
- 将主链路关键故障注入标准化

关键交付物：
- simulated-line 主场景集合
- 故障注入字典与用例落地
- 结构化 evidence bundle 产出稳定化

### 4.4 Phase 4：补性能与 HIL 收敛
目标：
- 建立 performance / stress / soak 基线
- 收敛 HIL 至高价值场景
- 形成 nightly 与专项 profile 的长期运行机制

关键交付物：
- `08 / 09` 文档
- performance baseline 初版
- HIL smoke / closed-loop / case-matrix 收敛版

---

## 5. 30/60/90 Day Plan

### 5.1 0-30 天
目标：冻结基准骨架并建立第一批正式判定物。

重点事项：
- 完成前 6 份基准文档
- 识别 P0 模块
- 为 `runtime-execution`、`motion-planning`、`process-path`、`dispense-packaging` 建立最小 contracts / golden
- 冻结 fixture / golden / evidence 目录规则

完成标志：
- 基准骨架已成文
- P0 主链至少具有可执行的最小 contracts / golden 面
- 失败可输出结构化最小证据

### 5.2 31-60 天
目标：建立 simulated-line 主场景与主风险链 fault injection 面。

重点事项：
- 建立 `success / block / rollback / recovery / archive` 五类场景骨架
- 为 `runtime-service`、`workflow`、`runtime-execution` 补足状态与恢复路径测试
- 建立 protocol compatibility 正式样本集
- 为 main/nightly profile 接入更完整的证据产出

完成标志：
- simulated-line 成为高层自动化主战场
- 主风险链路已有标准 fault injection 集
- acceptance 场景可自动运行并归档证据

### 5.3 61-90 天
目标：建立性能基线与 HIL 收敛结构。

重点事项：
- 固化 small / medium / large performance profile
- 建立 cold / hot / singleflight baseline
- 引入 2h soak
- 收敛 HIL smoke / closed-loop / case-matrix
- 建立第二批模块接入计划

完成标志：
- performance baseline 已可比较
- HIL 不再承载大部分正确性回归
- 第二批模块接入计划已明确

---

## 6. Module Rollout Plan

### 6.1 第一批模块
第一批模块应为主风险链与当前正式测试聚合面中的关键 owner：
- `runtime-execution`
- `motion-planning`
- `process-path`
- `dispense-packaging`
- `apps/runtime-service`
- `workflow`
- `dxf-geometry`
- `job-ingest`

### 6.2 第二批模块
第二批模块应补齐上游链与证据链短板：
- `topology-feature`
- `process-planning`
- `coordinate-alignment`
- `trace-diagnostics`

### 6.3 第三批模块
第三批模块聚焦观测投影与交互一致性：
- `hmi-application`
- 其他尚未进入正式测试聚合面的辅助模块

### 6.4 延后模块
若某模块：
- 风险较低
- 依赖边界尚未冻结
- 当前并非主风险链关键路径
可延后接入，但必须在路线图中显式标识原因。

---

## 7. Milestones

### 7.1 M1 骨架完成
判定条件：
- 基准文档第一批完成
- 目录、owner、suite、label、simulated-line / HIL 边界冻结

### 7.2 M2 判定物完成
判定条件：
- P0 模块具备 contracts / golden / evidence 最小面
- fixture / golden / evidence 目录和规则已落地

### 7.3 M3 simulated-line 主场景完成
判定条件：
- 五类 acceptance 场景骨架可执行
- 主风险链 fault injection 可在 simulated-line 中复现

### 7.4 M4 HIL 收敛完成
判定条件：
- HIL 被收敛为 smoke / closed-loop / case-matrix
- 大部分高层正确性验证已前移至 simulated-line
- performance / soak baseline 已初步建立

---

## 8. Deliverables

### 8.1 文档交付物
至少包括：
- 基准文档集
- 执行指南
- 路线图

### 8.2 测试代码交付物
至少包括：
- P0 模块 contracts / golden / state-machine / fault-injection 测试
- simulated-line 主场景骨架
- main/nightly 可调度入口

### 8.3 fixture/golden 交付物
至少包括：
- 主风险链最小样本集
- 正式 golden 集
- 最小 evidence bundle 模板

### 8.4 CI/profile 交付物
至少包括：
- PR / main / nightly / HIL profile 说明
- 阻断与非阻断规则
- 报告路径约定

---

## 9. Acceptance Criteria

### 9.1 每阶段验收标准
每阶段必须同时满足：
- 文档完成
- 结构完成
- 至少一批正式测试落地
- 报告与证据可产出

### 9.2 不通过条件
出现以下任一情况，该阶段不视为完成：
- 只有文档，无实际测试落地
- 有测试，但无正式证据输出
- 有 simulated-line / HIL 承载面，但边界未冻结
- baseline 无法比较或不可复现

### 9.3 延期条件
若发生以下情况，可触发延期但必须补充说明：
- 关键 owner 边界重构中
- 核心协议未冻结
- 关键硬件环境不可用
- 大样本或性能 profile 尚不可稳定复现

---

## 10. Risks And Dependencies

### 10.1 环境依赖
包括：
- 构建环境
- Python/runtime 依赖
- CI 节点能力
- 报告落盘路径

### 10.2 硬件依赖
包括：
- HIL 所需板卡与 IO
- 真机 smoke 环境
- 传感器与反馈链

### 10.3 人力依赖
包括：
- 模块 owner 参与
- 测试平台 owner 参与
- Agent 工作流接入

### 10.4 历史债务依赖
包括：
- legacy 目录未清
- fixture / golden 历史散落
- 旧 profile 与新 profile 并存
- 临时脚本事实化

---

## 11. Governance Loop

### 11.1 周期性复审
建议按固定周期复审：
- P0 模块测试面
- baseline 漂移
- simulated-line / HIL 边界执行情况
- performance 趋势

### 11.2 baseline 更新点
只有在以下节点允许集中更新基准：
- 架构重构完成
- 协议版本升级完成
- 样本集升级完成
- profile 语义变更获批

### 11.3 偏差处理机制
当实际执行偏离路线图时，应记录：
- 偏差原因
- 影响范围
- 补偿措施
- 是否需要修订基准文档
