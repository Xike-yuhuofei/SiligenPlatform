# 机台前质量门禁 v1

更新时间：`2026-04-02`

## 1. 机台前必须通过的层

真实机台测试前，必须先通过：

- `L0` 静态与构建门禁
- `L1` 单元测试
- `L2` 模块契约测试
- `L3` 集成测试
- `L4` 模拟全链路 E2E

对应 gate：

- `quick-gate` 必须全绿，作为提交与 PR 基础阻断
- `full-offline-gate` 必须全绿，作为机台前主发现层
- 若本轮涉及性能/长稳风险，`nightly-performance` 相关样本必须无阻断

## 2. 机台测试不允许首次发现的问题

以下问题若在 `L5` 首次暴露，直接判定为前置层失职：

- 参数签名漂移
- 无约束 fake/mock 吞错
- preview 主链不通
- UI 假成功 / 假在线
- 配置 schema 明显不兼容
- 离线样本无法重放
- gateway launch contract 缺失
- preview source / kind / hash 主契约不一致

## 3. L5 的正式角色

`L5` 只承担闭环确认，不承担低级 bug 主发现职责。

`L5` 必须声明：

- 已通过的前置离线层
- 本轮 HIL 样本
- 安全前提
- operator 介入点
- 中止原因
- 证据包路径

## 4. 机台前阻断清单

以下任一项未通过，禁止上机：

- `tests/reports/static/pyright-report.*`
- `tests/reports/static/no-loose-mock-report.*`
- `tests/reports/workspace-validation.json|md`
- `tests/reports/**/case-index.json`
- `tests/reports/**/validation-evidence-bundle.json`
- `tests/reports/**/evidence-links.md`
- `full-offline-gate` 对应 integration / simulated-line 报告

## 5. 执行原则

- 机台测试前优先离线复现并固定样本
- 每个 bug fix 先补失败测试，再修代码，再纳入正式 gate
- HIL 失败必须能回链到离线层证据与样本 ID
