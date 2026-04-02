# 05 Baseline Suite Label CI

## 1. Purpose

### 1.1 文档目的
本文档冻结项目的 suite、label、CI profile 与阻断策略，统一：
- 正式 suite 名称与职责
- label 语义
- suite 与 label 的关系
- PR / main / nightly / HIL 调度方式
- 正式执行与证据策略

### 1.2 与根级入口的关系
本项目正式测试调度以根级入口为准。  
本文档不创建新的事实入口，而是约束现有入口的行为与扩展方式。

---

## 2. Suite Model

### 2.1 suite 定义
suite 是从验证面角度对测试集合进行的正式编排单位。  
suite 用于：
- 执行入口选择
- CI profile 组织
- 报告聚合
- 大类验证面治理

### 2.2 项目标准 suite 清单
项目标准 suite 以正式入口为准，至少包括：
- `all`
- `apps`
- `contracts`
- `e2e`
- `protocol-compatibility`
- `performance`

### 2.3 各 suite 的职责
#### `all`
承载该 profile 下允许执行的全部正式测试集合。

#### `apps`
承载应用/宿主侧相关测试。

#### `contracts`
承载模块级与跨模块的正式契约验证面。

#### `e2e`
承载 simulated-line 与 HIL 相关端到端验证。

#### `protocol-compatibility`
承载协议与兼容性验证面。

#### `performance`
承载性能、压力、长稳相关验证面。

### 2.4 suite 的准入与退役
新增或退役 suite 必须满足：
- 与现有 suite 语义不重叠
- 有固定入口与固定报告策略
- 已同步更新文档与调度配置

---

## 3. Label Model

### 3.1 label 定义
label 是从运行成本、风险、语义、环境等维度对测试进行的正交标记。

### 3.2 size labels
- `small`
- `medium`
- `large`

这些标签表示测试规模、运行时间和环境复杂度，而不是业务归属。

### 3.3 semantic labels
推荐正式标签：
- `contracts`
- `protocol`
- `e2e-sim`
- `hil`
- `perf`
- `soak`
- `stress`
- `safety-critical`
- `nightly-only`
- `flaky-forbidden`

### 3.4 risk / environment labels
如有需要，可补充：
- `hardware-required`
- `simulator-required`
- `mock-only`

但必须避免与 suite 语义重叠。

### 3.5 label 组合规则
一个测试可以同时拥有多个 label，例如：
- `medium + protocol`
- `large + e2e-sim`
- `large + hil + safety-critical`

---

## 4. Suite vs Label

### 4.1 正交关系
- suite：解决“按哪类验证面组织运行”
- label：解决“从成本/风险/环境如何过滤”

### 4.2 何时用 suite
当需要：
- 聚合某一类正式验证面
- 组织报告
- 形成正式入口参数
时，使用 suite。

### 4.3 何时用 label
当需要：
- 细粒度过滤
- 跨 suite 选取某类测试
- 标记运行条件
时，使用 label。

### 4.4 过滤执行规则
任何基于 label 的过滤都不得破坏 suite 的语义边界。  
label 是筛选器，不是替代 suite 的并行体系。

---

## 5. CI Profiles

### 5.1 PR profile
目标：
- 快速、稳定、强阻断

推荐包含：
- L0 治理门禁
- `small` 为主的模块内测试
- 核心 `contracts`
- 少量关键 `medium` 集成 smoke

不应默认包含：
- 大量 `large`
- HIL
- 长稳 soak

### 5.2 main profile
目标：
- 在主干级别提供更完整的回归面

推荐包含：
- 全量 `contracts`
- `protocol-compatibility`
- 更多 `medium`
- 关键 simulated-line smoke
- 关键 performance baseline

### 5.3 nightly profile
目标：
- 扩大覆盖面与长期风险暴露

推荐包含：
- `large`
- 大样本 profile
- 更多故障矩阵
- stress / soak
- 更广的 simulated-line acceptance

### 5.4 HIL profile
目标：
- 验证硬件相关高价值风险

推荐包含：
- hardware smoke
- closed-loop
- case matrix
- 必要时的硬件 soak

---

## 6. Blocking Policy

### 6.1 必须阻断的失败
以下失败默认必须阻断：
- L0 治理门禁失败
- 正式 `contracts` 失败
- 正式 `protocol-compatibility` 失败
- PR profile 中的核心 smoke 失败
- safety-critical 场景失败

### 6.2 可报告但不阻断的失败
以下测试可按 profile 配置为仅报告、不阻断：
- exploratory large performance
- 长时间 soak
- 某些 nightly-only 项

### 6.3 临时豁免规则
任何临时豁免必须：
- 明确原因
- 明确范围
- 明确时限
- 明确回收条件

### 6.4 flaky 测试处理规则
正式体系中不得长期容忍 flaky 测试。  
发现 flaky 应优先：
1. 归因
2. 降级为非阻断 profile
3. 修复后再恢复

---

## 7. Execution Policy

### 7.1 默认入口
正式执行只允许通过受控入口触发，不允许将临时脚本视为事实主入口。

### 7.2 参数约定
入口参数必须稳定、可文档化、可由 CI 可靠调用。

### 7.3 并行策略
- `small` / `medium` 可优先并行
- `large` / `hil` 需按环境资源受控执行

### 7.4 失败重试策略
重试只应用于环境不稳定或基础设施偶发问题；不得用重试掩盖确定性逻辑缺陷。

### 7.5 超时策略
每类 profile 应有默认超时上限，并对长稳类测试单独定义超时与中止策略。

---

## 8. Reporting Policy

### 8.1 每个 profile 的最小报告要求
每个正式 profile 都必须输出：
- execution summary
- 失败测试清单
- 关键 artifact 引用
- 结构化结果

### 8.2 PR 证据要求
PR 级别至少应给出：
- 通过/失败状态
- 阻断测试详情
- 最小失败证据链接或路径

### 8.3 main/nightly 证据要求
应额外输出：
- baseline diff
- profile 级汇总
- 趋势或历史比较（如适用）

### 8.4 HIL 证据要求
必须包含：
- 执行环境信息
- 场景摘要
- 关键事件 timeline
- 失败归因所需最小证据

---

## 9. Adding New Suites Or Labels

### 9.1 新增条件
新增 suite 或 label 必须满足：
- 现有语义无法表达
- 不造成重叠与混乱
- 有明确长期用途

### 9.2 评审要求
必须同步评审：
- 文档
- 调度配置
- 报告影响
- 命名合理性

### 9.3 命名规则
- suite 名称应简短、稳定、语义清晰
- label 名称应避免歧义与缩写冲突

### 9.4 文档更新要求
新增或变更后，必须同步更新本基准文档及相关执行文档。

---

## 10. Anti-Patterns

### 10.1 suite 名泛滥
### 10.2 label 语义重叠
### 10.3 CI profile 与 suite 直接耦合失控
### 10.4 一次失败多次手工解释
### 10.5 用 profile 差异掩盖契约不一致

---

## 11. Normative Rules

### 11.1 MUST
- MUST 使用正式 suite 组织验证面
- MUST 使用 label 进行正交过滤
- MUST 为每个正式 profile 定义阻断与证据策略
- MUST 将 suite / label 变化纳入文档治理

### 11.2 SHOULD
- SHOULD 保持 suite 数量稳定
- SHOULD 让 label 精简且可组合
- SHOULD 将高风险测试显式标记为 safety-critical

### 11.3 MUST NOT
- MUST NOT 用 label 替代 suite 治理
- MUST NOT 让临时 CI 逻辑成为事实标准
- MUST NOT 长期容忍未治理的 flaky 测试
