# 06 Baseline Simulated Line And HIL

## 1. Purpose

### 1.1 文档目的
本文档冻结 simulated-line 与 HIL 的角色分工、边界、场景范围与证据要求，避免：
- 过早把问题推向真机
- 两套环境语义不一致
- acceptance 场景与硬件场景边界混乱

### 1.2 为什么需要单独文档
simulated-line 与 HIL 都位于较高层测试面，但它们的目标、成本、可控性和诊断方式完全不同，必须单独治理。

---

## 2. Simulated-Line Role

### 2.1 定义
simulated-line 是在受控模拟环境下，对运行时链路、协议链路、场景链路进行端到端验证的正式测试面。

### 2.2 目标
simulated-line 的主要目标是：
- 让 acceptance 场景自动化
- 支持可重复的故障注入
- 提供高质量结构化证据
- 在不依赖真实硬件的前提下覆盖大部分联机风险

### 2.3 适用场景
适合验证：
- success / block / rollback / recovery / archive
- 协议时序
- 重试/超时/中断
- 宿主装配与 runtime 协作
- 恢复与回滚语义

### 2.4 非适用场景
不适合单独证明：
- 真实板卡协议电气级时序
- 真实 IO 电平与反馈
- 真实安全链联锁
- 真实机械反馈依赖问题

---

## 3. Simulated-Line Architecture

### 3.1 fake-gateway
模拟 gateway 行为，负责：
- 请求接收
- 响应返回
- 协议事件派发
- 故障注入入口

### 3.2 fake-controller
模拟控制器/执行器行为，负责：
- 命令消费
- 运行状态变更
- 假定反馈生成

### 3.3 fake-io-board
模拟 IO 读写与反馈缺失、抖动等行为。

### 3.4 fake-machine-readiness
模拟设备就绪、未就绪、就绪恢复等前提状态。

### 3.5 fake-clock
提供可控时钟，以便稳定复现 timeout、retry、delay 等时序问题。

### 3.6 fault-injector
提供统一故障注入能力，包括但不限于：
- timeout
- duplicate-response
- out-of-order-response
- disconnect-mid-flight
- not-ready
- feedback-missing
- superseded-artifact

### 3.7 event-recorder
记录：
- 关键事件序列
- 状态迁移
- 故障触发点
- 证据 bundle 所需的 timeline

---

## 4. Simulated-Line Required Scenarios

### 4.1 success
最小主成功闭环，验证系统在正常条件下从触发到完成的标准路径。

### 4.2 block
验证前置条件不足、配置不满足、设备未就绪等阻断路径。

### 4.3 rollback
验证执行中断、冲突或失败后系统如何回滚到安全状态。

### 4.4 recovery
验证故障后恢复流程，包括 reset、retry、重新进入 ready 的路径。

### 4.5 archive
验证场景完成后产物归档、状态归档或生命周期收敛行为。

---

## 5. HIL Role

### 5.1 定义
HIL 是将真实硬件控制链的一部分与可控环境结合，用于验证模拟环境无法充分证明的高价值风险的正式测试面。

### 5.2 目标
HIL 的目标是：
- 验证真实板卡/IO/安全链风险
- 验证真实硬件反馈时序
- 验证硬件初始化、握手、home、enable、stop 等关键闭环

### 5.3 适用场景
适合验证：
- hardware smoke
- closed-loop 控制链
- case matrix 中的关键硬件故障
- 长稳运行中的硬件相关问题

### 5.4 非适用场景
不适合用来代替：
- 模块内逻辑验证
- 契约 drift 检测
- 大量协议对象级回归
- GUI 主正确性证明

---

## 6. HIL Profiles

### 6.1 hardware smoke
目标：
- 验证设备最小可启动、可连接、可执行最小链路

### 6.2 closed-loop
目标：
- 验证真实控制器与反馈链形成闭环的关键行为

### 6.3 case-matrix
目标：
- 验证少量高价值硬件故障矩阵，如：
  - not-ready
  - feedback-missing
  - stop path
  - timeout path

### 6.4 long-run / soak（如适用）
目标：
- 验证长期运行中的硬件稳定性与状态收敛行为

---

## 7. Coverage Boundary Between Simulated-Line And HIL

### 7.1 先 simulated-line 后 HIL
任何可在 simulated-line 明确证明的场景，应先在 simulated-line 建立正式验证，再考虑进入 HIL。

### 7.2 哪些必须在 simulated-line 证明
至少包括：
- 五类 acceptance 主场景
- 大多数协议时序故障
- 大多数恢复路径
- 宿主与 runtime 的组合语义

### 7.3 哪些必须进入 HIL
至少包括：
- 真实硬件启动链
- 真实 IO/反馈链
- 真实安全停机路径
- 真实板卡相关时序风险

### 7.4 哪些不得直接跳入 HIL
不得直接把以下问题留给 HIL：
- 模块内状态机问题
- 单模块契约漂移
- 纯协议对象兼容问题
- 可通过 fake-clock / fault-injector 稳定复现的问题

---

## 8. Failure Injection Mapping

### 8.1 simulated-line 支持的故障类型
推荐支持：
- invalid-input
- missing-config
- mode-mismatch
- timeout
- duplicate-response
- out-of-order-response
- disconnect-mid-flight
- not-ready
- feedback-missing
- superseded-artifact
- retry-exhausted
- rollback-required

### 8.2 HIL 支持的故障类型
HIL 应优先承载：
- 真实未就绪
- 真实反馈缺失/异常
- 真实 stop / reset / interlock 故障
- 真实硬件时序超时

### 8.3 两者共享的故障语义
若同一种故障在两种环境中都存在，应尽量共享：
- 故障名称
- 故障分类
- 报告字段
- evidence 语义

### 8.4 证据对齐要求
simulated-line 与 HIL 证据应能在以下维度对齐：
- 场景名称
- 失败分类
- 关键事件序列
- 最终状态
- 归因入口

---

## 9. Evidence Requirements

### 9.1 Simulated-Line 最小证据
每次正式 simulated-line 运行至少应输出：
- summary.json
- timeline.json
- artifacts/*
- report.md

### 9.2 HIL 最小证据
每次正式 HIL 运行至少应输出：
- 运行环境摘要
- summary.json
- timeline.json
- 关键硬件事件/反馈摘要
- report.md

### 9.3 跨环境对比规则
若同一场景同时存在 simulated-line 与 HIL 版本，两者应至少在下列方面可比较：
- 场景目标
- 成功判定条件
- 失败分类
- 关键状态收敛

---

## 10. Anti-Patterns

### 10.1 把 HIL 当主战场
### 10.2 模拟不闭环
### 10.3 只在真机复现状态机问题
### 10.4 两套故障语义不一致
### 10.5 HIL 证据弱于 simulated-line

---

## 11. Normative Rules

### 11.1 MUST
- MUST 明确区分 simulated-line 与 HIL 职责
- MUST 优先在 simulated-line 建 acceptance 主场景
- MUST 为 simulated-line / HIL 输出正式 evidence bundle
- MUST 保持两者故障语义尽量对齐

### 11.2 SHOULD
- SHOULD 用 fake-clock 和 fault-injector 提高场景可控性
- SHOULD 让 HIL 聚焦少量高价值场景
- SHOULD 为同名场景维护跨环境可比性

### 11.3 MUST NOT
- MUST NOT 用 HIL 替代 simulated-line 的主覆盖面
- MUST NOT 跳过 simulated-line 直接扩大量 HIL
- MUST NOT 让 simulated-line 成为无故障注入的“假联机”
