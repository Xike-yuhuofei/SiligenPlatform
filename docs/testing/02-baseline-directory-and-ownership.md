# 02 Baseline Directory And Ownership

## 1. Purpose

### 1.1 文档目的
本文档冻结测试目录结构、测试代码归属、模块内测试与仓库级测试的边界规则，避免：
- 测试代码散落
- 责任归属不清
- 仓库级 tests 侵入业务 owner
- 临时脚本演化为事实入口

### 1.2 与总纲文档的关系
本文档是 `01-baseline-architecture.md` 的结构落地文档。  
总纲定义“分层与原则”，本文档定义“目录与归属”。

---

## 2. Ownership Model

### 2.1 模块 owner 定义
模块 owner 指对某一业务模块的实现、边界、契约及其模块内测试负最终责任的目录归属方。

### 2.2 测试 owner 定义
测试 owner 分为两类：
- 模块测试 owner：负责该模块内 `tests/`
- 仓库级测试 owner：负责根级 `tests/` 中的跨 owner 验证面

### 2.3 仓库级测试 owner 定义
仓库级测试 owner 只负责：
- 跨模块集成验证
- simulated-line / HIL
- performance / soak
- 共享 fixture / report 编排

仓库级测试 owner 不替代模块 owner 对模块正确性的责任。

### 2.4 owner 冲突处理原则
若某测试同时满足：
- 可明确归属单一模块
- 无需跨 owner 装配

则优先放模块内。  
仅当测试目标天然跨 owner 时，才可进入仓库级 `tests/`。

---

## 3. Directory Blueprint

### 3.1 模块内 tests/ 目录蓝图
推荐骨架：

```text
<module>/
  tests/
    unit/
    contracts/
    golden/
    compatibility/      # 如适用
    fault-injection/    # 如适用
    state-machine/      # 如适用
    recovery/           # 如适用
```

模块可按需要裁剪，但不得引入语义重叠的 ad-hoc 目录。

### 3.2 仓库级 tests/ 目录蓝图
推荐骨架：

```text
tests/
  integration/
    scenarios/
    protocol-compatibility/
  e2e/
    simulated-line/
    hardware-in-loop/
  performance/
    profiles/
    stress/
    soak/
  fixtures/
    dxf/
    golden/
    protocol/
    faults/
    timing/
  reports/
```

### 3.3 fixtures/ 目录蓝图
共享输入资产必须进入根级 `tests/fixtures/` 或模块内正式 fixture 位置，不得散落到脚本临时目录。

### 3.4 reports/ 目录蓝图
正式报告输出必须进入受控报告目录，不得输出到随机临时路径。

### 3.5 示例目录树
示意：

```text
modules/
  process-path/
    tests/
      unit/
      contracts/
      golden/
tests/
  integration/
    scenarios/
  e2e/
    simulated-line/
  performance/
    profiles/
```

---

## 4. Module-Level Test Placement Rules

### 4.1 unit 的落点
所有纯逻辑、纯算法、纯映射测试必须优先放模块内 `tests/unit/`。

### 4.2 contracts 的落点
所有只验证该模块输入、输出、错误、观测契约的测试必须放模块内 `tests/contracts/`。

### 4.3 golden 的落点
当 golden 的语义完全属于单一模块时，必须优先放模块内 `tests/golden/`。

### 4.4 compatibility 的落点
若兼容性问题只涉及该模块自有 schema / payload / serializer，则放模块内 `tests/compatibility/`。  
若兼容性跨多个 owner，则转入仓库级 `tests/integration/protocol-compatibility/`。

### 4.5 fault-injection 的落点
- 模块内部可模拟的故障：放模块内
- 跨模块、跨宿主、跨运行时故障：放仓库级 integration / e2e

---

## 5. Repository-Level Test Placement Rules

### 5.1 integration 的落点
`tests/integration/` 只承载跨 owner 的组合验证。

### 5.2 protocol-compatibility 的落点
所有跨 owner 的协议兼容验证放 `tests/integration/protocol-compatibility/`。

### 5.3 e2e 的落点
`tests/e2e/` 只承载 acceptance 场景和模拟/硬件场景，不承载模块内逻辑证明。

### 5.4 performance 的落点
`tests/performance/` 承载：
- profile
- stress
- soak
- baseline diff

### 5.5 soak/stress 的落点
不得在模块内目录临时堆放 soak/stress 测试；必须纳入仓库级 performance 面。

---

## 6. Canonical Test Roots

### 6.1 canonical root 的定义
指被根级测试聚合入口正式承认并纳入调度的测试根目录。

### 6.2 当前 canonical root 范围
项目当前的 canonical test roots 应以根级聚合配置为准。  
新增 root 必须显式接入根级聚合。

### 6.3 新 root 准入规则
新增 canonical root 需满足：
- 有清晰 owner
- 有明确验证目标
- 有固定输出与证据规则
- 不与现有 root 语义重叠

### 6.4 root 弃用规则
当某 root 被合并、迁移或退役时，必须同步：
- 更新根级聚合
- 更新文档
- 清理旧目录
- 维护迁移说明

---

## 7. Canonical Subdirectories

### 7.1 integration 的 canonical subdirectories
`tests/integration/` 仅允许正式承载于：
- `scenarios/`
- `protocol-compatibility/`

### 7.2 e2e 的 canonical subdirectories
`tests/e2e/` 仅允许正式承载于：
- `simulated-line/`
- `hardware-in-loop/`

### 7.3 performance 的 canonical subdirectories
`tests/performance/` 仅允许正式承载于：
- `profiles/`
- `stress/`
- `soak/`

### 7.4 禁止随意新增平级目录
若需新增平级目录，必须先更新基准文档与根级入口，再落地目录。

---

## 8. Cross-Boundary Rules

### 8.1 仓库级 tests 不得回写业务 owner
仓库级 tests 可消费业务 owner 产物与契约，但不得替代模块内单元/契约测试。

### 8.2 模块 tests 不得承担跨 owner 装配
一旦测试目标天然跨 owner，必须上升到根级 `tests/`。

### 8.3 不得跨目录放 fixture
fixture 必须放在模块正式目录或根级共享目录；禁止放在随机工具脚本旁边。

### 8.4 不得把临时脚本当正式测试入口
只有纳入正式入口、正式目录、正式报告策略的脚本，才可视为正式测试组成部分。

---

## 9. New Module Onboarding Rules

### 9.1 新模块接入最小要求
每个新模块至少应具备：
- `tests/unit/`
- `tests/contracts/`
- 最小 fixture 或最小 golden

### 9.2 最小目录骨架
推荐最小骨架：

```text
<module>/
  tests/
    unit/
    contracts/
```

### 9.3 接入根级聚合的条件
模块接入根级聚合前必须满足：
- 测试目录已稳定
- 输出与证据规则已定义
- 责任边界清晰
- 不会破坏现有 profile

### 9.4 P0/P1/P2 接入差异
- P0：应优先进入正式聚合
- P1：先在模块内成熟，再接入聚合
- P2：先建立最小骨架，按路线图择机接入

---

## 10. Forbidden Directory Patterns

### 10.1 ad-hoc tests
禁止在未纳入正式规范的目录随意新增 `tmp_tests/`、`misc-tests/`、`exp-tests/` 等事实测试面。

### 10.2 临时 golden 散落
禁止将 golden 文件长期留在临时目录或报告目录中。

### 10.3 报告输出到随机位置
正式测试不得将报告长期输出到不可预测路径。

### 10.4 integration 目录承载模块内细节
integration 不得退化为“某模块内部逻辑回归仓库”。

---

## 11. Examples

### 11.1 正确示例
- 模块字段映射测试放模块内 `tests/contracts/`
- 多模块协议兼容测试放 `tests/integration/protocol-compatibility/`
- success / recovery 场景放 `tests/e2e/simulated-line/`

### 11.2 错误示例
- 将模块内部状态机小测试写到 `tests/e2e/`
- 将跨模块装配测试写在某一模块自己的 `tests/unit/`
- 把 golden 放到脚本生成目录而不归档

### 11.3 重构示例
若某仓库级测试本质已退化为单模块验证，应迁回模块内并移除仓库级重复承载。

---

## 12. Normative Rules

### 12.1 MUST
- MUST 明确测试 owner
- MUST 使用正式目录承载正式测试
- MUST 优先将单模块测试放模块内
- MUST 将跨 owner 验证放仓库级 `tests/`

### 12.2 SHOULD
- SHOULD 使用统一目录命名
- SHOULD 为每类测试维持清晰、稳定的子目录
- SHOULD 为新模块提供最小测试骨架

### 12.3 MUST NOT
- MUST NOT 在随机目录建立事实测试入口
- MUST NOT 让仓库级 tests 演化为模块内部回归堆场
- MUST NOT 长期保留未归档的 fixture / golden / reports
