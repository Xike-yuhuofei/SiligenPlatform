---
name: hmi-production-operator-test
description: "Drive real production from the HMI to simulate operator workflow, surface operator-facing issues, and separate operator problem findings from traceability conclusions. Use for HMI production test, 人工操作排查, 操作问题清单, and 点胶机生产过程问题发现."
---

# Purpose

本 skill 的唯一职责是：通过 HMI 主链路模拟人工完成一次真实生产，并系统发现人工操作时会遇到的问题。

它不把“链路能跑通”当作唯一目标，而是同时输出：

1. 操作问题清单
2. 链路追溯一致性结论

# Use this skill when

- 用户要求通过 HMI 对真实设备做生产测试。
- 目标是排查人工操作点胶机时会遇到的问题。
- 自动化脚本只能控制 UI，不能绕过 HMI 直接调用接口。
- 需要同时沉淀操作证据和 runtime/gateway 追溯证据。

# Do not use this skill when

- 当前只是离线测试、mock 测试或无机台验证。
- 当前目标只是判断 HIL 是否通过，而不关心人工操作体验。
- 当前目标只是根因定位，尚未进入真实操作验证。
- 当前允许跳过 HMI 直接调用接口。

# Inputs

- 目标 DXF 或生产对象
- HMI 入口与操作脚本入口
- 设备已具备直接联机条件的确认
- 本轮成功标准
- 需要收集的证据范围

# Required outputs

1. 测试目标与操作范围
2. 操作主线步骤
3. 操作问题清单
4. 链路追溯一致性结论
5. 关键证据路径
6. 中止条件与风险说明
7. 后续建议

# Hard prohibitions

- 不得绕过 HMI 直接通过接口触发生产。
- 不得把 summary-level alignment 夸大成严格一一对应。
- 不得只给“通过/失败”而不输出人工操作问题。
- 不得把“操作问题排查”与“追溯一致性证明”混成一个模糊结论。

# Workflow rules

## 0. 先检查 UI 自动化入口是否真的覆盖生产路径

若用户要求“脚本必须控制 UI，不能绕过 HMI”，则必须先确认控制脚本是否已经支持：

1. DXF 导入
2. preview / prepare
3. production start
4. 执行中观察
5. 完成后收尾或下一单恢复

若当前脚本只支持 `operator_preview`、`home_move` 或其他局部动作，而不支持真实 production start：

- 本轮结论必须为 `BLOCK`
- 问题类型记为 `自动化能力缺口`
- 不得回退为直接调用接口或非 HMI 入口

## 1. 先冻结目标，再执行

先明确本轮是：

- 以操作问题发现为主
- 以生产成功为主
- 还是以严格追溯一致性为主

若用户同时要三者，默认主输出拆成两份：

- 操作问题清单
- 追溯一致性结论

## 2. 必须走人工主线

默认操作主线：

1. 启动 HMI
2. 确认联机与设备状态
3. 导入 DXF
4. 执行 preview / prepare
5. 发起 production start
6. 观察执行中状态
7. 观察完成或中止后的界面与日志
8. 尝试再次发起下一单或恢复操作

除非用户明确缩小范围，否则不要只测 happy path。

## 3. 操作问题必须按类型归档

至少分四类：

- 阻塞类：流程走不通、按钮不可用、状态条件不满足但无法继续
- 误导类：界面提示与真实状态不一致、按钮可点但不该点、错误提示不可执行
- 低效类：步骤冗余、重复确认、隐含前提过多
- 恢复类：失败后无法继续、必须重启、状态残留、下一单无法发起

## 4. 严格区分两种结论

### A. 操作问题结论

回答：人工操作时会不会卡住、误判、绕路、无法恢复。

### B. 追溯一致性结论

回答：轴/阀日志与规划数据是否严格一一对应。

若生产成功但 `coord-status-history` 为空，或证据只支持 count / coverage / completion：

- 操作主线可判“生产可执行”
- 追溯一致性必须判“证据不足”或“未证明严格一一对应”

## 5. 证据必须同时覆盖 UI 与执行链

至少收集：

- HMI 操作时间线
- HMI 截图或录像
- runtime 日志
- gateway 日志
- 报告目录
- `coord-status-history`
- 发生异常时的界面状态与报错文案

## 6. 旧语义不得误导人工问题判断

若 HMI 仍被本地 `recipe_id` / `version_id` 选择阻断，而当前 authoritative truth 已转到 runtime `production_baseline`，应记录为真实操作问题，而不是归咎于操作员。

# Output format

严格使用以下结构输出：

## HMI 生产操作测试单

### 1. 测试目标
- 
- 

### 2. 操作范围
- DXF / 生产对象：
- HMI 入口：
- 是否允许绕过 HMI：不允许

### 3. 操作主线
1.
2.
3.

### 4. 操作问题清单
- 类型：
  现象：
  影响：
  复现步骤：
  当前证据：
  建议归属层：

### 5. 追溯一致性结论
- 生产执行结论：
- 严格一一对应结论：
- 证据边界：

### 6. 控制脚本能力结论
- 入口脚本：
- 是否覆盖 production start：
- 结论：允许 / 阻断
- 若阻断，缺口：

### 7. 关键证据
- HMI：
- runtime：
- gateway：
- report：
- coord-status-history：
- 异常截图/录像：

### 8. 中止条件与风险
- 
- 

### 9. 建议下一步
- 先修操作阻塞问题 / 先补追溯证据 / 先做最小回归

# Handoff

- 上游：
  - `xike-clarify`
  - `online-validation-gate-and-evidence`
- 下游：
  - `incident-intake`
  - `targeted-fix-and-regression`
  - `closeout-branch`
