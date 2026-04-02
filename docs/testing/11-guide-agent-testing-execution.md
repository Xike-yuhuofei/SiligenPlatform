# 11 Guide Agent Testing Execution

## 1. Purpose

### 1.1 文档目的
本文档将 `SiligenPlatform` 项目的分层测试体系翻译为面向 AI Agent 的可执行规则，用于规范：
- Agent 在测试建设中的职责
- Agent 新增、修改、迁移测试时的流程
- Agent 修改 fixture / golden / baseline / CI profile 时的限制
- Agent 输出说明、证据与自检的最低要求

### 1.2 适用 Agent 范围
本文档适用于：
- Codex CLI Agent
- Claude Code / CLI Agent
- 其他能直接修改仓库文件的自动化编码代理

人类开发者亦可参照本文档审视 Agent 输出是否合规。

---

## 2. Agent Responsibilities

### 2.1 读取基准文档
在开始任何正式测试建设任务前，Agent 必须先读取与当前任务相关的基准文档，而不是仅依据用户当前提示猜测规则。

### 2.2 识别模块 owner
Agent 必须先识别任务对应的模块 owner 或仓库级承载面，再决定测试放置位置。
若 owner 未识别清楚，禁止直接创建新的正式测试目录。

### 2.3 识别测试层级
Agent 必须判断目标任务属于：
- 模块内小测试
- 模块契约测试
- 仓库级 integration
- simulated-line E2E
- HIL
- performance / soak

层级未判清前，不得开始实施。

### 2.4 生成证据与报告
Agent 不只负责“让测试能跑”，还必须负责：
- 选择或创建合适的判定物
- 补齐 evidence bundle
- 明确更新 golden 或 fixture 的理由
- 在变更说明中写出影响范围

### 2.5 不越权修改边界
Agent 不得以“方便实现”为理由：
- 越过模块边界把测试放到错误目录
- 在仓库级 tests 中补模块内细节测试
- 直接扩张 HIL 以绕过 simulated-line 缺失
- 用 ad-hoc 脚本替代正式入口

---

## 3. Required Reading Order

### 3.1 必读文档顺序
所有正式测试任务的默认阅读顺序：
1. `01-baseline-architecture.md`
2. `02-baseline-directory-and-ownership.md`
3. `03-baseline-module-test-matrix.md`
4. `04-baseline-fixture-golden-evidence.md`
5. `05-baseline-suite-label-ci.md`

### 3.2 按任务类型附加读取项
- 若任务涉及 simulated-line 或 HIL，必须追加读取 `06-baseline-simulated-line-and-hil.md`
- 若任务涉及性能、压力、长稳，必须追加读取 `08-baseline-performance-and-soak.md`
- 若任务涉及 fixture / golden / baseline 更新，必须追加读取 `09-baseline-test-data-lifecycle.md`
- 若任务是规划或排期性质，必须追加读取 `10-roadmap-test-implementation.md`

### 3.3 未读取禁止实施的场景
若 Agent 未读取相关基准文档，则不得：
- 新增正式目录
- 修改 golden
- 修改 CI profile
- 新增 HIL case
- 将测试迁移到更高层级

---

## 4. Task Type Routing

### 4.1 新增模块测试
路由规则：
- 先确认模块属于产物型、运行时控制型还是观测交互型
- 再根据 `03-baseline-module-test-matrix.md` 选择最小测试面
- 优先补模块内测试，再考虑 integration

### 4.2 补契约测试
路由规则：
- 优先放模块内 `contracts/`
- 必要时补共享 fixture / golden
- 若跨 owner 边界，才升级到 integration

### 4.3 补 simulated-line
路由规则：
- 先确认是否属于 acceptance 五类场景
- 先证明模块内与 integration 已经有基础判定物
- 再进入 simulated-line 场景实现

### 4.4 补 HIL
路由规则：
- 必须先说明该问题为何不能由 simulated-line 证明
- 必须选择 `hardware smoke`、`closed-loop`、`case-matrix` 中的一个 profile
- 必须定义最小证据输出

### 4.5 更新 golden / fixture
路由规则：
- 必须说明为什么旧资产失效
- 必须给出对应代码或契约变更依据
- 必须说明更新是否影响历史 baseline

### 4.6 更新 CI/profile
路由规则：
- 必须说明归属的 suite / label / profile
- 必须说明是否阻断 PR / main / nightly / HIL
- 必须补充报告与超时策略说明

---

## 5. Agent Workflow

### 5.1 识别目标模块
Agent 必须先回答：
- 目标模块是谁
- 上下游是谁
- 属于哪个 owner
- 当前是否已在 canonical test roots 中

### 5.2 确认测试层
Agent 必须先说明：
- 为什么该测试属于当前层
- 是否可下沉到更低层
- 是否会错误上推到更高层

### 5.3 选择样本与故障注入
Agent 必须从标准样本类型中选择：
- `small`
- `normal`
- `dirty`
- `boundary`
- `failure`

若是故障用例，还必须从统一故障字典中选择注入点。

### 5.4 生成测试
生成测试时，Agent 必须同步生成：
- 测试名称与归属目录
- 判定物
- 样本引用
- 失败证据要求
- suite / label 建议

### 5.5 生成/更新报告
若任务会影响正式报告口径，Agent 必须同步：
- 明确报告输出路径
- 更新 README 或索引
- 在说明中标注新增证据类型

### 5.6 自检
提交前，Agent 必须执行自检，至少检查：
- 是否放在正确目录
- 是否有稳定断言或 golden
- 是否引入了动态字段污染
- 是否需要更新文档

### 5.7 提交说明
Agent 的变更说明至少应包含：
- 目标模块
- 测试层级
- 新增测试类型
- 样本/故障注入点
- 证据类型
- 未覆盖的剩余风险

---

## 6. Change Rules

### 6.1 修改 fixture 的规则
Agent 修改 fixture 前必须说明：
- 旧 fixture 的问题是什么
- 新 fixture 覆盖了哪些新增风险
- 是否影响已有 golden 与 performance baseline

### 6.2 修改 golden 的规则
Agent 修改 golden 前必须说明：
- 是逻辑修复导致的合法变化，还是误更新
- 影响哪些模块、哪些 suite
- 是否已屏蔽动态字段
- 是否需要人工复核

### 6.3 修改 baseline 的规则
任何 baseline 文档改动都必须被视为治理级改动。
Agent 不得在“顺手修测试”时顺带改 baseline 规则，除非任务明确要求。

### 6.4 修改 CI profile 的规则
Agent 修改 CI/profile 前必须说明：
- 为什么现有 profile 不足
- 是否增加执行成本
- 是否改变阻断语义
- 是否会影响现有报告归档

---

## 7. Required Output Contract

### 7.1 代码输出要求
Agent 输出的测试代码必须：
- 落在正确目录
- 命名清晰
- 使用正式入口接入
- 不引入临时私有脚本作为唯一运行方式

### 7.2 文档输出要求
若变更影响以下任一内容，Agent 必须同步更新文档：
- 新增正式测试目录
- 新增或变更 suite / label
- 新增报告类型
- 新增或更新 fixture / golden / baseline

### 7.3 报告输出要求
若是 integration / e2e / performance / HIL 级别改动，Agent 必须说明：
- 报告目录
- 关键 artifact
- 最小 evidence bundle

### 7.4 说明文字要求
Agent 的文字说明不得只写“添加测试”“修复测试”。
必须写出：
- 测试目标
- 证明的风险点
- 为什么放在这一层
- 还有什么未做

---

## 8. Self-Check Checklist

### 8.1 边界自检
- 测试是否放在正确 owner 目录
- 仓库级测试是否只承载跨 owner 验证
- 是否误把模块内测试上推到 integration/E2E

### 8.2 判定物自检
- 是否有稳定断言
- 是否有 golden 或结构化 evidence
- 是否存在动态字段污染

### 8.3 证据自检
- 失败时能否输出 summary / timeline / artifact dump / report
- 是否便于复盘
- 是否便于 CI 归档

### 8.4 风险自检
- 该变更覆盖了哪些风险
- 哪些风险仍未覆盖
- 是否引入新的测试债务

---

## 9. Forbidden Actions

### 9.1 越层补测试
禁止在明明应由 lower layer 证明的问题上，直接增加更高层测试来掩盖缺口。

### 9.2 无文档依据修改 baseline
禁止在没有明确任务要求时修改 baseline 治理文档。

### 9.3 直接覆盖 golden
禁止无说明地整体覆盖 golden 输出。

### 9.4 跳过 simulated-line 直接扩 HIL
禁止为了“更真实”而绕开 simulated-line 的设计与判定物建设。

### 9.5 新增 ad-hoc 目录与入口
禁止创建未被基准文档承认的正式测试目录、脚本或运行入口。

---

## 10. Prompting Templates

### 10.1 新增模块测试提示模板
- 目标模块
- 目标层级
- 目标风险
- 参考 baseline 文档
- 预期样本类型
- 预期判定物
- 预期输出目录

### 10.2 补契约测试提示模板
- 输入/输出契约
- 失败契约
- 观测契约
- 样本与 golden
- 目录与 suite

### 10.3 更新 golden 提示模板
- 变更原因
- 影响范围
- 动态字段掩码规则
- 证据与人工复核要求

### 10.4 生成报告提示模板
- 报告输出位置
- 最小 evidence bundle
- 关键诊断字段
- 归档与索引更新

---

## 11. Review Expectations

### 11.1 Agent 自评要求
Agent 在完成任务后应自评：
- 层级是否正确
- 判定物是否充分
- 证据是否充分
- 是否需要后续补充工作

### 11.2 人工复核要求
以下情形必须优先人工复核：
- golden 批量更新
- suite / label / CI profile 变更
- HIL 新增或扩张
- baseline 治理文档变更

### 11.3 合并前检查要求
合并前应至少确认：
- 正式入口可运行
- 新测试被正确纳入 suite/profile
- 文档与实现一致
- 报告路径与 evidence 可用

---

## 12. Normative Rules

### 12.1 MUST
- Agent 在实施前必须识别 owner、层级、样本类型与判定物。
- Agent 在修改 fixture / golden / CI profile 时必须附带明确理由。
- Agent 在 integration 及以上层级改动时必须说明证据输出。

### 12.2 SHOULD
- Agent 应优先补 lower layer 测试，再扩 higher layer。
- Agent 应优先复用现有 fixtures、golden、fault dictionary 与 suite/profile。
- Agent 应在说明中诚实标出未覆盖风险。

### 12.3 MUST NOT
- Agent 不得越权改变测试治理边界。
- Agent 不得无说明覆盖资产与 baseline。
- Agent 不得用临时脚本绕过正式入口与正式文档。
