# shared/logging

`shared/logging/` 是稳定日志抽象与公共日志类型的 canonical destination，用于承接跨模块复用的 logging contract、formatter 和 trace context 基础能力。

## 当前状态

- 本目录当前保留职责说明与目录占位，用于约束跨模块日志基础能力的稳定边界。
- 通用基础类型继续收敛在 `shared/kernel/`，模块级日志落盘与诊断适配实现在 `modules/trace-diagnostics/adapters/logging/`。
- 新的共享 logging 抽象只能进入 canonical 根，不得重新引入第二套历史 owner 面。

## 禁止承载

- 具体 sink/adapter 实现和运行时落盘实现
- 查询、回放、报警记录等 trace 业务模型本体
