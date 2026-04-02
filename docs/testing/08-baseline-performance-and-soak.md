# 08 Baseline Performance And Soak

## 1. Purpose

### 1.1 文档目的
本文档冻结 `SiligenPlatform` 项目的性能、压力与长稳测试基准，统一：
- 性能语义
- 样本规模语义
- baseline 建立与回归判定规则
- stress / soak 的场景边界
- 性能证据与报告要求

### 1.2 与正确性测试的关系
性能测试不替代正确性测试。

本项目中：
- 正确性测试用于证明“结果是否正确”
- 性能测试用于证明“在给定输入规模、给定环境、给定调度约束下，结果是否以可接受代价产生”
- stress / soak 用于证明“在持续压力或长时间运行下，系统是否维持可接受行为与可诊断性”

性能测试的前提是：
- 对应链路已有最小正确性证明
- 输入样本与证据结构已冻结
- profile 的环境信息可记录、可比较、可复现

---

## 2. Scope

### 2.1 performance
`performance` 指在受控环境下，对指定链路、指定规模样本、指定 profile 进行的性能测量与回归比较。

常见指标包括：
- latency
- throughput
- peak memory
- CPU time / wall time
- queueing / join / cache 行为

### 2.2 stress
`stress` 指通过提高输入规模、并发度、资源占用或事件密度，观察系统在高负载下的边界行为。

stress 的目标不是“必须全部成功”，而是明确：
- 系统在边界下如何退化
- 失败是否可控
- 证据是否足够诊断
- 是否触发不允许的状态或资源泄漏

### 2.3 soak
`soak` 指在较长时间内重复执行固定或半固定场景，以验证：
- 稳定性
- 资源泄漏风险
- 状态累积问题
- 周期性故障
- 长时间运行后的诊断能力

### 2.4 非适用范围
本文档不负责：
- 单个测试用例设计方法
- fixture 生命周期治理
- simulated-line / HIL 的架构细节
- PR/main/nightly/HIL 的总调度策略

上述内容由其他基准文档规定。

---

## 3. Performance Semantics

### 3.1 cold
`cold` 指在无预热、无缓存命中、无既有 authority artifact、无既有进程态优势下的首轮执行语义。

`cold` 适用于衡量：
- 首次导入或首次预览
- 首次规划
- 首次运行时启动
- 首次协议装配

### 3.2 hot
`hot` 指在同一进程、同一 profile、相同或相近输入条件下，预热完成后的执行语义。

`hot` 适用于衡量：
- 重复预览
- 已缓存 artifact 的重复确认
- 已装配宿主下的重复运行
- 重复协议装配或查询

### 3.3 singleflight
`singleflight` 指多个请求针对同一权威对象或同一资源键进行并发访问时，系统应当通过 join / dedupe / coalescing 等机制避免重复计算，并保证：
- 输出结果一致
- 结果所有权清晰
- 等待与加入行为可观测
- 无不必要的重复工作

### 3.4 throughput
`throughput` 指单位时间内完成的有效工作量，适用于：
- 任务提交
- 状态查询
- 事件处理
- 批量规划

### 3.5 latency
`latency` 指从请求发出到关键结果可用之间的时间。

需区分：
- request-to-accept
- request-to-first-result
- request-to-final-result
- request-to-visible-state

### 3.6 peak memory
`peak memory` 指执行 profile 期间观察到的峰值内存占用，需与：
- 输入规模
- 并发度
- profile 类型
- 长时间运行时段
相结合解释。

---

## 4. Sample Scale Definitions

### 4.1 small
`small` 表示最小可重复、可代表核心链路语义的输入规模。

`small` 的要求：
- 执行快
- 可作为 PR/profile 的常规基线
- 能稳定暴露链路级性能回归

### 4.2 medium
`medium` 表示接近真实业务常规负载的输入规模。

`medium` 的要求：
- 能反映日常使用下的性能行为
- 应纳入 main 或 nightly 的正式比较面
- 能覆盖缓存、组装、序列化、状态聚合等中层成本

### 4.3 large
`large` 表示接近或达到已知大样本级别的输入规模。

`large` 的要求：
- 用于捕获高规模回归
- 用于观察内存峰值与延迟增长趋势
- 不应默认进入所有 PR profile

### 4.4 扩展档位规则
如需新增 `xlarge` 或其他规模档位，必须满足：
- 规模语义清晰
- 样本来源稳定
- profile 中有明确使用场景
- 文档与 baseline 同步更新

---

## 5. Baseline Policy

### 5.1 baseline 建立
建立 baseline 时必须冻结以下信息：
- profile 名称
- 输入样本标识与版本
- 执行入口与参数
- 环境信息
- 关键指标
- 证据路径

### 5.2 baseline 更新
允许更新 baseline 的情况包括：
- 链路结构发生正式重构
- 输出语义或 authority 结构发生已批准变更
- 样本发生版本升级
- 历史 baseline 已被证明失真或不可复用

### 5.3 回归判定
性能回归必须基于基准比较，不得只凭单次感知判断。

回归判定至少包括：
- 指标名称
- baseline 值
- 当前值
- 变化比例或绝对差值
- 环境信息
- 变化解释

### 5.4 允许波动范围
允许波动范围应按 profile 单独定义，不允许全仓使用同一固定阈值。

至少区分：
- PR 快速 profile：容忍度更高，但需更稳定
- nightly 深度 profile：容忍度更低，可记录更丰富指标
- soak：更关注趋势与资源泄漏，不以单次延迟为唯一指标

---

## 6. Performance Profiles

### 6.1 DXF 预览链
应至少覆盖：
- `artifact.create`
- `plan.prepare`
- `preview.snapshot`
- preview authority reuse / cache reuse / singleflight join

### 6.2 工艺规划链
应至少覆盖：
- process path 生成
- motion planning 生成
- 关键摘要或 authority artifact 产出
- 规划失败的分类与短路开销

### 6.3 打包链
应至少覆盖：
- payload 组装
- 序列化
- 大载荷路径下的峰值内存
- 协议版本差异下的成本变化

### 6.4 运行时启动链
应至少覆盖：
- host bootstrap
- runtime assembly
- mode switch
- first ready / first visible state

### 6.5 事件风暴链
应至少覆盖：
- 高频状态更新
- 高频查询
- 高频日志/诊断事件
- UI / host 侧的投影更新压力

---

## 7. Stress Policy

### 7.1 并发压力
应覆盖：
- 同一 artifact key 的并发请求
- 多 artifact 并行生成
- 运行时重复启动/停止
- 高频状态查询与事件订阅

### 7.2 资源压力
应覆盖：
- 大样本导入
- 低内存余量场景
- 高日志量场景
- 多 profile 同时运行场景

### 7.3 长时间压力
应覆盖：
- 重复导入/规划/确认/停止循环
- 长时间运行时状态刷新
- 持续事件积累
- 持续报告落盘

### 7.4 边界资源测试
边界资源测试应回答：
- 系统如何退化
- 是否进入不允许状态
- 是否产生可归类失败
- 是否留下完整证据

---

## 8. Soak Policy

### 8.1 2h soak
`2h soak` 作为最小长稳验证面，用于验证：
- 基本循环运行稳定性
- 无明显内存持续增长
- 无显著状态漂移
- 报告与证据仍可正常产出

### 8.2 8h soak
`8h soak` 面向接近生产节拍的中长时验证，用于发现：
- 周期性退化
- 资源泄漏
- 累积性错误
- 定时相关问题

### 8.3 24h soak（如适用）
仅在目标链路已具备明确价值时引入。

`24h soak` 不应作为常规入口默认执行，应定位为专项验证 profile。

### 8.4 退出与报告规则
soak 必须记录：
- 实际运行时长
- 成功/失败计数
- 中途异常点
- 资源趋势摘要
- 退出原因

---

## 9. Reporting And Evidence

### 9.1 JSON 报告
至少包含：
- profile 信息
- 样本信息
- 环境信息
- 关键指标
- baseline diff
- 失败摘要

### 9.2 Markdown 报告
用于人类快速阅读，应包括：
- 本次 profile 概览
- 与 baseline 比较
- 关键异常
- 后续建议

### 9.3 baseline diff
若存在 baseline，必须输出结构化 diff，不得只输出“变快/变慢”这类描述性语言。

### 9.4 趋势比较
对于 soak 与长期保留 profile，建议保留趋势字段或历史摘要，以便发现：
- 波动放大
- 资源持续上升
- 周期性异常

---

## 10. Anti-Patterns

### 10.1 不固定样本
不同样本混用而不标识版本，会使 profile 失去可比较性。

### 10.2 baseline 任意更新
无说明地覆盖 baseline，会使回归检测失效。

### 10.3 压测无证据
只记录“跑过了”而不输出关键指标与异常摘要，不视为正式 performance 结果。

### 10.4 用单次结果代替趋势
对 soak 或高波动 profile，仅凭单次结果下结论是不被允许的。

---

## 11. Normative Rules

### 11.1 MUST
- MUST 为每个正式 performance profile 冻结样本、入口、指标与证据输出。
- MUST 将 `small / medium / large` 与 `cold / hot / singleflight` 语义分开管理。
- MUST 基于 baseline diff 判定回归，不得仅依赖描述性结论。
- MUST 对 soak 输出运行时长、异常点与资源趋势摘要。

### 11.2 SHOULD
- SHOULD 优先从主风险链路建立 performance baseline。
- SHOULD 将 `singleflight` 作为 authority / cache / dedupe 热点链路的正式验证面。
- SHOULD 在 nightly 或专项 profile 中运行更重的 large/stress/soak 测试。

### 11.3 MUST NOT
- MUST NOT 在未固定样本与环境信息的前提下宣称存在正式 baseline。
- MUST NOT 用 HIL 替代大多数 performance 回归验证。
- MUST NOT 仅因单次偶发波动就覆盖 baseline。
