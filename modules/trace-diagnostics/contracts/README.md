# trace-diagnostics contracts

`modules/trace-diagnostics/contracts/` 承载 `M10 trace-diagnostics` 的模块 owner 专属契约。

## 契约范围

- `trace_diagnostics/contracts/IDiagnosticsPort.h` 冻结 generic diagnostics sink contract，以及 `DiagnosticInfo` / `HealthReport` / `PerformanceMetrics` DTO。
- `trace_diagnostics/contracts/DiagnosticTypes.h` 冻结跨 machine/runtime 共用的 diagnostics result / issue / trigger statistics 类型。
- logging bootstrap 输入、logger naming、最小配置与初始化结果的最小契约口径。
- 诊断日志基础字段的命名约束，不扩张为 trace/query/archive/audit 数据模型。
- `trace_diagnostics/contracts/ValidationEvidenceBundle.h` 冻结最小 evidence bundle 结构。

## 边界约束

- 本目录声明 logging bootstrap 与 generic diagnostics sink 的 canonical contract surface；adapter 实现留在模块内部，不作为 external truth。
- 不承接 `ITestRecordRepository` / `ITestConfigurationPort` / `ICMPTestPresetPort` / `TestDataTypes` 等 hardware-test diagnostics contracts；它们改由 `apps/runtime-service` bootstrap public shell 持有。
- 仅放置 `M10 trace-diagnostics` owner 专属契约，不放跨模块长期稳定公共契约。
- 跨模块通用日志能力应维护在 `shared/logging/`。
- 禁止在模块外再维护第二套 `M10` owner 专属契约事实。

## 当前对照面

- `shared/logging/`
- `apps/trace-viewer/`
