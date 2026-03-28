# application

`modules/trace-diagnostics/application/` 仅暴露 logging bootstrap 的 consumer surface。

- canonical public surface：`include/application/services/trace_diagnostics/LoggingServiceFactory.h`
- 当前职责是基于 `LogConfiguration` 创建 `ILoggingService`，并返回初始化结果
- 本轮不提供 trace query、archive、audit facade 或独立读取 use case
