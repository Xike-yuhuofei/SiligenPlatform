# trace-diagnostics contracts

`modules/trace-diagnostics/contracts/` 承载 `M10 trace-diagnostics` 的模块 owner 专属契约。

## 契约范围

- logging bootstrap 输入、logger naming 与初始化结果的最小契约口径。
- `spdlog` adapter 接线所需的配置兼容语义。
- 诊断日志基础字段的命名约束，不扩张为 trace/query/archive/audit 数据模型。
- `trace_diagnostics/contracts/ValidationEvidenceBundle.h` 冻结最小 evidence bundle 结构。

## 边界约束

- 本目录只声明 logging bootstrap / adapter 相关边界，不声明 trace/query/archive/audit 全量 owner 能力。
- 仅放置 `M10 trace-diagnostics` owner 专属契约，不放跨模块长期稳定公共契约。
- 跨模块通用日志能力应维护在 `shared/logging/`。
- 禁止在模块外再维护第二套 `M10` owner 专属契约事实。

## 当前对照面

- `shared/logging/`
- `apps/trace-viewer/`
