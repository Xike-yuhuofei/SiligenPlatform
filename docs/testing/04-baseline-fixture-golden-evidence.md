# 04 Baseline Fixture Golden Evidence

## 1. Purpose

### 1.1 文档目的
本文档冻结测试输入资产、golden 资产、evidence bundle 与报告目录的统一规则，确保：
- 测试可机器判定
- 失败可归档、可复盘
- 不同层级测试使用统一证据语言
- golden、fixture、报告不随意散落与漂移

### 1.2 与其他文档的关系
本文档与以下文档强关联：
- `03-baseline-module-test-matrix.md`：定义哪些模块必须具备 golden / evidence
- `05-baseline-suite-label-ci.md`：定义各 profile 的执行与报告要求
- `06-baseline-simulated-line-and-hil.md`：定义 simulated-line / HIL 的最小证据要求

本文档不定义测试用例设计方法，但定义“测试资产与证据必须长什么样”。

---

## 2. Asset Taxonomy

### 2.1 `fixture`
fixture 是测试输入资产，用于驱动测试运行。常见形式包括：
- DXF 样本
- 协议样本
- 配置样本
- 故障样本
- 标定样本
- 运行环境样本

### 2.2 `golden`
golden 是期望输出资产，用于与测试实际产出进行稳定对比。常见形式包括：
- 结构化 JSON
- 文本化 dump
- 协议 payload dump
- 路径规划摘要
- 证据结构样本

### 2.3 `evidence`
evidence 是一次测试运行的结果证据，用于：
- 判定失败
- 诊断归因
- 追溯环境
- 支撑复盘

### 2.4 `baseline`
baseline 是被正式接受的参考基线，用于：
- golden 基线
- performance 基线
- 证据结构基线

### 2.5 `report artifact`
report artifact 指运行后生成的正式报告与附属产物，包括：
- `summary.json`
- `timeline.json`
- `report.md`
- `artifacts/*`
- raw logs

---

## 3. Fixture Rules

### 3.1 fixture 分类
fixture 至少应按语义分类：
- `dxf/`
- `protocol/`
- `config/`
- `faults/`
- `timing/`
- `calibration/`（如适用）

### 3.2 fixture 存放位置
- 单一模块专属 fixture：放模块内正式 fixture 位置
- 跨模块共享 fixture：放根级 `tests/fixtures/`
- 不得将正式 fixture 放在临时脚本目录旁边

### 3.3 fixture 命名规范
fixture 文件名应：
- 表达用途
- 表达场景或特征
- 避免使用无语义随机名

推荐模式：
- `<domain>-<scenario>-<variant>.<ext>`
- `<feature>-<condition>-<size>.<ext>`

### 3.4 fixture 元数据要求
若 fixture 语义复杂，应附带元数据说明，至少描述：
- 来源
- 用途
- 适用模块
- 规模类别
- 预期成功/失败语义

### 3.5 fixture 最小质量要求
正式 fixture 必须：
- 可重现
- 来源可追溯
- 不包含无关噪声
- 有稳定的测试用途

---

## 4. Golden Rules

### 4.1 golden 适用范围
golden 适用于：
- 中间产物稳定可比较的模块
- 协议/载荷/摘要对象
- 状态投影结果
- 报告结构

### 4.2 golden 存放位置
- 模块专属 golden：放模块内 `tests/golden/`
- 跨模块共享 golden：放根级 `tests/fixtures/golden/` 或正式共享位置

### 4.3 golden 命名规范
golden 文件名应能回答：
- 对应哪个模块/场景
- 对应哪类输入
- 对应哪类输出

推荐模式：
- `<module>-<scenario>-expected.<ext>`
- `<flow>-<variant>-golden.<ext>`

### 4.4 golden 更新规则
更新 golden 必须同时满足：
- 变更理由明确
- 对应实现变更或契约变更明确
- 附带新旧差异证据
- 已通过必要的人工复核

### 4.5 golden 失效规则
满足以下任一条件，golden 应被视为失效候选：
- 依赖的 schema 已升级
- 输入 fixture 已变化
- 动态字段未被正确屏蔽
- 输出语义不再对应当前正式契约

---

## 5. Evidence Bundle Contract

### 5.1 `summary.json`
`summary.json` 是最小摘要文件，至少应包含：
- suite
- scenario / case id
- result（pass / fail / blocked / skipped）
- start/end 时间或运行时长
- 关键失败分类
- 最终状态
- 主要归因入口

### 5.2 `timeline.json`
`timeline.json` 用于记录关键事件序列，至少应包含：
- 事件时间或顺序索引
- 事件名
- 来源
- 关联状态变更
- 故障触发点（如适用）

### 5.3 `artifacts/*`
`artifacts/` 用于存放正式附属产物，例如：
- payload dump
- plan summary
- path summary
- config snapshot
- protocol transcript
- screenshot / capture（如适用）

### 5.4 `report.md`
`report.md` 用于提供人类可快速阅读的运行摘要，至少应包含：
- 测试目标
- 输入与环境
- 最终结果
- 关键失败/成功证据链接
- 后续动作建议（如适用）

### 5.5 raw logs
raw logs 用于保留原始上下文，但：
- raw logs 不能替代 `summary.json`
- raw logs 不能替代 `timeline.json`
- raw logs 不能作为唯一判定物

### 5.6 screenshot / capture / dump（如适用）
对于 HIL、HMI、协议调试等场景，可额外产出：
- 屏幕截图
- 波形截图
- 协议抓包
- 设备状态导出

但这些只能作为补充证据，不能替代结构化摘要。

---

## 6. Dynamic Fields Masking

### 6.1 时间戳字段
所有随运行变化的时间戳应：
- 在 golden 对比中被屏蔽
- 或转换为可比较的稳定表达

### 6.2 UUID / 随机标识
随机生成的 id、uuid、token、session id 等字段必须在对比前掩码化。

### 6.3 路径字段
受环境影响的绝对路径字段应：
- 标准化
- 相对化
- 或在 golden 中进行掩码处理

### 6.4 环境相关字段
受机器、用户名、工作目录、时区等影响的字段应统一屏蔽或标准化。

### 6.5 掩码规则
掩码规则必须：
- 可重复执行
- 与测试实现一起受控
- 不能破坏关键语义字段

---

## 7. Comparison Rules

### 7.1 文本对比
适用于：
- 简单摘要
- 小型报告
- 稳定文本协议片段

要求：
- 明确是否忽略空白
- 明确是否忽略行顺序

### 7.2 结构化 JSON 对比
适用于：
- summary
- timeline
- payload summary
- 规划摘要

要求：
- 先做规范化
- 再做字段级比较
- 动态字段必须提前掩码

### 7.3 二进制/协议 dump 对比
适用于：
- payload
- serializer output
- wire-level sample

要求：
- 明确版本
- 明确 header / body / checksum 的比较策略

### 7.4 数值容差对比
适用于：
- 坐标
- 规划数值
- 几何摘要

要求：
- 明确绝对容差
- 明确相对容差
- 不得在无文档说明的前提下随意放宽阈值

### 7.5 顺序敏感/不敏感对比
对比前必须明确：
- 该对象是否顺序敏感
- 若不敏感，允许使用排序后再比较
- 若敏感，必须保留原始顺序比较

---

## 8. Minimum Failure Evidence Requirements

### 8.1 unit 失败最小证据
至少应包含：
- 失败断言
- 输入摘要
- 实际/期望差异

### 8.2 contracts 失败最小证据
至少应包含：
- 契约对象摘要
- 差异字段列表
- golden/schema 引用

### 8.3 integration 失败最小证据
至少应包含：
- 场景摘要
- 关键上下游输入输出
- 失败链路位置
- summary + timeline

### 8.4 e2e 失败最小证据
至少应包含：
- scenario 名称
- 最终状态
- 故障分类
- summary + timeline + artifacts + report

### 8.5 performance 失败最小证据
至少应包含：
- profile 名称
- baseline 对比结果
- 样本规模
- 指标 diff
- 环境摘要

### 8.6 HIL 失败最小证据
至少应包含：
- 环境摘要
- summary
- timeline
- 关键硬件相关附属证据（如 capture / logs）

---

## 9. Report Structure

### 9.1 报告目录结构
正式报告应进入受控目录，建议按以下维度组织：
- 日期
- suite / profile
- 场景
- 运行 id

### 9.2 运行 ID 规则
每次正式运行应生成唯一运行 ID，用于关联：
- summary
- timeline
- artifacts
- logs

### 9.3 归档规则
正式报告应可归档，并支持：
- 按日期查找
- 按 suite/profile 查找
- 按场景查找
- 按运行 ID 查找

### 9.4 清理规则
清理报告时必须保证：
- 不删除仍被引用的 baseline 证据
- 保留最小可追溯窗口
- 不影响近期回归对比

---

## 10. Anti-Patterns

### 10.1 只存日志不存 summary
只保留 raw logs 会导致：
- 机器无法快速判定
- 人工诊断成本高
- 失败归因入口不稳定

### 10.2 golden 带动态字段
golden 中保留动态字段会导致：
- 无意义 diff
- false failure
- 基线失真

### 10.3 报告输出路径随机
随机输出路径会破坏：
- 可追溯性
- CI 收集
- 历史对比

### 10.4 失败不能复盘
若失败不产出结构化证据，则该测试不具备正式治理价值。

---

## 11. Templates

### 11.1 fixture 清单模板
至少应记录：
- 名称
- 分类
- 来源
- 适用模块
- 样本类型
- 用途说明

### 11.2 golden 清单模板
至少应记录：
- 名称
- 对应输入 fixture
- 适用模块
- 对比规则
- 动态字段掩码规则

### 11.3 evidence bundle 模板
至少应包含：
- `summary.json`
- `timeline.json`
- `artifacts/`
- `report.md`
- raw logs 引用

---

## 12. Normative Rules

### 12.1 MUST
- 正式测试必须尽量输出结构化 evidence bundle。
- 使用 golden 的测试必须定义动态字段屏蔽规则。
- 正式 fixture / golden / evidence 必须放在受控目录。
- E2E 与 HIL 失败必须至少产出 `summary.json` 与 `timeline.json`。

### 12.2 SHOULD
- 复杂 fixture 应附带元数据说明。
- 结构化对象应优先使用结构化比较，而不是纯文本比较。
- 数值型 golden 应显式定义容差规则。

### 12.3 MUST NOT
- 不得仅以 raw logs 作为正式失败证据。
- 不得在无理由、无差异说明的情况下直接覆盖 golden。
- 不得将动态路径、随机 id、时间戳原样写入正式 golden 并直接比较。
