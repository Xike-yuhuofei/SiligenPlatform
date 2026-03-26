# trace-diagnostics contracts

`modules/trace-diagnostics/contracts/` 承载 `M10 trace-diagnostics` 的模块 owner 专属契约。

## 契约范围

- 追溯事件记录、诊断日志条目与审计关联键的语义约束。
- HMI/验证消费侧读取追溯信息所需的最小输入/输出口径。
- 告警、错误与诊断分类字段的命名与兼容约束。

## 边界约束

- 仅放置 `M10 trace-diagnostics` owner 专属契约，不放跨模块长期稳定公共契约。
- 跨模块通用日志能力应维护在 `shared/logging/`。
- 禁止在模块外再维护第二套 `M10` owner 专属契约事实。

## 当前对照面

- `shared/logging/`
- `apps/trace-viewer/`
