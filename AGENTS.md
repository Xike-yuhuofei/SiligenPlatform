# SiligenSuite Agent Rules

## 回应原则（最高优先级）
- 必须保持严格中立、专业和工程导向的回应风格。
- 判断只能基于事实、逻辑和技术依据，不得因用户语气而迎合、附和或改变结论。
- 禁止使用模棱两可的表述，结论必须明确且可执行。

## 语言
- 必须使用简体中文与用户沟通。

## 语言分层约定

- 凡是与代码对象、命令、路径、工具、脚本、目录、配置键、协议字段、类型名、类名、函数名、模块名、仓库结构直接耦合的内容，优先写英文。
- 凡是与业务语义、机械工艺、架构边界、职责判断、误用防护、风险说明、验证结论、停机条件、流程意图直接相关的内容，优先写中文。
- 凡是必须触发准确的 metadata，优先使用英文主描述，并补充中文别名、中文触发词或中文近义表述。
- 如果同一条内容同时涉及代码对象和业务语义，采用“英文对象 + 中文解释”的写法，不要反过来。
- skill 的 `name`、frontmatter `description`、`agents/openai.yaml` 中的 `display_name`、`short_description`、`default_prompt` 都视为高触发敏感 metadata，必须优先保证英文触发准确性。
- `display_name` 使用纯英文，不附带中文别名；中文别名放在 `description`、`short_description` 或 `default_prompt` 中。

## 仓库使命

本仓库是工业点胶机上位机 / 监控控制软件仓库。首要工程目标是：

1. 执行正确性
2. 设备状态一致性
3. 全链路契约完整性
4. 可重复的离线验证
5. 可诊断性与可追溯性
6. 清晰模块边界下的可维护性

这不是通用 CRUD 或纯 Web 项目。不要把设备行为、预览行为、执行行为、HMI 展示和诊断行为当成可互换概念。

## 仓库级运行规则

- `AGENTS.md` 只放长期稳定的仓库规则。
- 可重复、步骤敏感、带分支逻辑的工作流应下沉到 skill。
- 如果任务匹配现有 skill，优先使用 skill，而不是在普通对话里临时拼流程。
- 所有非平凡任务在实施前都必须先完成任务分类。

## Git 分支命名（强制）
- 所有新建分支必须使用统一格式：`<type>/<scope>/<ticket>-<short-desc>`。
- 不符合该格式的分支禁止继续开发，必须先重命名为合规名称。
- 详细规则、字段约束、示例与校验建议统一以 `docs/onboarding/git-branch-naming.md` 为准。

### 字段约束
- `type`：使用 `docs/onboarding/git-branch-naming.md` 中批准的白名单，当前包含 `feat`、`fix`、`refactor`、`docs`、`test`、`chore`、`perf`、`build`、`ci`、`revert`、`spike`。
- `scope`：使用稳定的小写模块或责任域名称，推荐优先采用 `docs/onboarding/git-branch-naming.md` 中的 project scopes。
- `ticket`：优先使用任务系统编号（如 `MC-142`、`BUG-311`、`ARCH-057`、`SPEC-021`）；若无外部任务系统，使用文档中定义的内部 ticket 前缀。
- `short-desc`：英文小写短描述，使用 kebab-case，例如 `debug-instrumentation`。

### 示例
- `chore/hmi/SS-142-debug-instrumentation`
- `fix/runtime/SS-205-startup-timeout`
- `chore/hmi/TASK-144-debug-instrumentation`
- `build/infra/TASK-122-upgrade-clang-cl`
- `ci/build/TASK-133-add-static-analysis`

## 任务类型

所有非平凡任务都必须先归类为以下四类之一：

1. `feature`
   - 新能力
   - 新契约
   - 新模块行为
   - 新 HMI / 系统行为
   - 绑定到新能力的计划性重构
2. `incident`
   - Bug
   - 回归
   - 性能下降
   - 预览 / 执行不一致
   - 运行时失败
   - 错误调查
   - 先根因后修复的工作
3. `boundary-audit`
   - 模块职责审查
   - `apps/` / `modules/` / `shared/` 边界审查
   - owner 漂移
   - 依赖方向审查
   - bridge / fallback / compat 污染审查
   - 结构治理
4. `online-validation`
   - 核心目标是对真实硬件做验证
   - 真实轴运动
   - 真实阀输出
   - 真实 IO 行为
   - 真实传感器行为
   - 真实设备状态迁移检查
   - 现场风险受控验证

如果任务类型不清晰，先运行 `task-classifier`。

## 强制门禁

### Gate 1：先澄清，再实现
未明确任务类型、目标、成功标准和预期范围前，不得开始编码。

### Gate 2：先冻结契约，再改语义
凡是涉及几何、工艺、轨迹、触发、执行、预览、诊断或设备状态的行为变化，必须先识别：
- 输入工件
- 输出工件
- `authority` 工件
- owner 层
- 只能消费语义的层

### Gate 3：先定义范围，再落编辑
改代码前必须明确：
- 允许修改什么
- 禁止修改什么
- 是否允许跨模块
- 什么算越界扩散

### Gate 4：先离线，再联机
默认先走离线验证。凡是可以离线验证的改动，不得把真实设备作为第一验证环境。

### Gate 5：先证据，再结论
不得把猜测当结论。必须用日志、代码路径、测试、工件和对照结果作为证据。

### Gate 6：先收尾，再宣称完成
“看起来改完”不算完成。只有当验证状态、剩余风险和文档影响都已明确，任务才算完成。

## 目录 owner 规则

### `apps/`
- 组合根
- 宿主层 wiring
- 应用入口
- UI host / runtime host / transport host
- 仅承载适配与编排

`apps/` 不得成为核心业务语义的长期 owner。

### `modules/`
- 主业务 / 领域 owner
- 核心工作流行为
- 核心规划 / 转换 / 执行语义
- 领域服务、策略与权威工作流逻辑

### `shared/`
- 中性共享契约
- 可复用工具
- 横切基础设施辅助
- 通用 DTO 或低层原语

`shared/` 不得累积本应属于领域 owner 的业务逻辑。

### `docs/`
- 架构契约
- 验证记录
- 运行说明
- 设计依据
- 迁移说明
- 测试策略与联机验证记录

## 契约规则

对任何有语义意义的变更，都必须识别并保护以下对象：

1. `semantic object`
   - 当前到底在改变什么语义
2. `authority artifact`
   - 下游链路应信任的唯一真值工件是什么
3. `owner layer`
   - 哪一层允许定义或重解释该语义
4. `consumer layers`
   - 哪些层只能消费、适配、展示或传输该语义
5. `no double truth`
   - 预览真值与执行真值不得在没有显式设计与文档说明的情况下分叉
6. `no hidden owner`
   - bridge、fallback、view、compat 层不得偷偷变成设备语义的真实 owner

## 验证阶梯

决定验证深度时，默认使用以下阶梯：

### L0：结构 / 静态检查
- 目录正确性
- 依赖方向合理性
- 契约 / 文档一致性
- 明显边界违规

### L1：单元与契约验证
- 纯逻辑
- DTO / schema / contract 检查
- 状态迁移检查
- 转换正确性

### L2：离线集成验证
- mocks
- fakes
- simulators
- replay
- 尽可能离线端到端

### L3：针对性回归
- 直接受影响路径
- 邻近路径
- 代表性样本
- 旧行为保持

### L4：性能验证
- 分段耗时
- 热点对比
- 基线对比
- 回归阈值检查

### L5：受限联机验证
- 只在足够通过离线门禁后进行
- 限动作 / 限范围 / 限输出 / 限风险
- 明确观察点
- 明确停机条件

### L6：收尾
- 证据摘要
- 残余风险摘要
- 文档同步摘要
- 下一步建议

不是每个任务都需要所有层级，但每个非平凡任务都必须明确写出哪些层级已执行，哪些被有意跳过。

## 默认分流

### 如果任务类型是 `feature`
1. `task-classifier`（若尚未分类）
2. `context-bootstrap`
3. `feature-clarify`
4. `contract-freeze`
5. `feature-spec-and-plan`
6. `change-scope-guard`
7. `task-breakdown`
8. `task-execute`
9. `post-change-closeout`

### 如果任务类型是 `incident`
1. `task-classifier`（若尚未分类）
2. `context-bootstrap`
3. `incident-intake`
4. `contract-freeze`（语义不清时）
5. `root-cause-analysis`
6. `change-scope-guard`
7. `targeted-fix-and-regression`
8. `post-change-closeout`

### 如果任务类型是 `boundary-audit`
1. `task-classifier`（若尚未分类）
2. `context-bootstrap`
3. `boundary-audit`
4. `change-scope-guard`
5. `refactor-plan-and-migrate`
6. `post-change-closeout`

### 如果任务类型是 `online-validation`
1. `task-classifier`（若尚未分类）
2. `context-bootstrap`
3. `online-validation-gate-and-evidence`
4. `post-change-closeout`

## 核心 skill 清单

### 共享底座
- `task-classifier`
- `context-bootstrap`
- `contract-freeze`
- `change-scope-guard`
- `post-change-closeout`

### 新功能流
- `feature-clarify`
- `feature-spec-and-plan`
- `task-breakdown`
- `task-execute`

### 故障修复流
- `incident-intake`
- `root-cause-analysis`
- `targeted-fix-and-regression`

### 架构治理流
- `boundary-audit`
- `refactor-plan-and-migrate`

### 联机验证流
- `online-validation-gate-and-evidence`

## Skill 选择原则

- 上述 15 个 skill 是默认骨架。
- 仓库中已有更窄、更专用的 skill 时，优先使用更专用的 skill。例如：`ss-incident-debug`、`ss-feature-deep-test`、`ss-mock-regression`、`ss-post-change-closeout`、`xike-code-explain` 以及 `speckit-*`。
- 不要在普通聊天指令里重复实现同一套流程。

## 编辑规则

1. 优先改正确的 owner 层，而不是改最方便的层。
2. 优先做最小、范围受控的修改，而不是大扫除式清理。
3. 不要把 bug 修复、新功能、重构、架构迁移和联机验证混在一次无控制的改动里。
4. 如果任务意外跨越语义层，立即停止并重新冻结范围或契约。
5. 如果 bridge / compat / fallback 层看起来像真实实现 owner，必须显式指出。
6. 如果预览路径与执行路径的派生逻辑不同，必须明确说明分叉点。
7. 高风险命令优先走 `scripts/validation/invoke-guarded-command.ps1`。
8. 正式验证优先使用根级 `build.ps1`、`test.ps1`、`ci.ps1` 及仓库校验脚本。

## 审查与报告要求

- 明确区分事实、推断和建议。
- 重要结论必须绑定代码证据、工件证据或验证证据。
- 证据不足时必须显式指出。
- 优先“最小充分修改”，不要把风格清理冒充成架构进展。
- 优先“owner 正确落点”，不要为了省事落到错误层。

## 非平凡工作计划规则

对以下任务，必须在实现前给出简短执行计划：

- 多步骤
- 跨模块
- 高风险
- 影响契约
- 影响状态机
- 影响设备行为

计划至少包含：

1. 任务类型
2. 成功标准
3. 受影响 owner / 层
4. 契约影响
5. 预期验证层级
6. 停止条件

## 分支与合并行为

- 实质性工作使用隔离分支，不要把主线当试验场。
- 进入可合并状态前，必须确认：
  - 目标仍在范围内
  - 相关验证已完成
  - 残余风险已声明
  - 文档影响已声明
  - 已产出 closeout

## 性能专项规则

如果任务和性能相关：

- 不要直接跳到优化
- 先识别耗时片段
- 先隔离责任阶段
- 尽量做可复现的 before / after 基线对比
- 未经明确批准，不要以“优化”为名改语义

性能问题默认属于 `incident`，除非它明确属于计划中的新功能或架构迁移。

## 联机专项规则

如果任务涉及真实硬件：

- 离线验证仍是默认第一门禁
- 执行前先定义观察点
- 执行前先定义停机 / 中止条件
- 变更与验证步骤都应受限
- 结果要记录成工程证据，而不是口头确认

联机验证不能替代缺失的离线验证。

## Done Definition

只有同时满足以下条件，任务才算完成：

1. 任务类型已正确识别
2. 受影响语义契约已理解或已明确声明不受影响
3. 代码落在正确的 owner 层
4. 范围保持受控，或者扩展已被重新批准
5. 相关验证层级已完成或已显式延后
6. 残余风险与未验证路径已声明
7. 文档影响已声明
8. 已产出 closeout 输出

缺少任一项，都不算完整完成。

## 失败处理规则

如果执行中发现：

- 任务分类错误
- owner 层判断错误
- `authority` 工件不清晰
- 所需改动超出已批准范围
- 实际需要联机验证
- 问题本质是架构性的而不是局部性的

停止当前路径，退回到正确的上游 skill，不要硬把错误流程推到底。

## 各阶段最小输出

### 澄清阶段
必须产出：
- 目标
- 非目标
- 成功标准
- 边界

### 契约阶段
必须产出：
- 语义对象
- `authority` 工件
- owner 层
- consumer-only 层

### 实施阶段
必须产出：
- 精确变更范围
- 受影响 owner
- 已执行的最小验证

### 审查 / 收尾阶段
必须产出：
- 变更摘要
- 证据摘要
- 残余风险
- 文档影响
- 下一步建议
