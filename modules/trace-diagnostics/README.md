# trace-diagnostics

`modules/trace-diagnostics/` 是 `M10 trace-diagnostics` 的 canonical owner 入口。

## Owner 范围

- logging bootstrap、诊断日志接线，以及模块内部 `spdlog` adapter 实现语义。
- 汇聚 `M10` owner 专属 logging/diagnostics contracts 边界。
- 承接 workflow planning、runtime-execution 与 runtime bootstrap 共用的 generic diagnostics sink contract：`trace_diagnostics/contracts/IDiagnosticsPort.h`。
- 本轮不声明 trace/query/archive/audit 全量 owner 能力。

## Owner 入口

- 构建入口：`modules/trace-diagnostics/CMakeLists.txt`（target：`siligen_module_trace_diagnostics`）。
- 模块契约入口：`modules/trace-diagnostics/contracts/README.md`。
- 测试入口：`modules/trace-diagnostics/tests/CMakeLists.txt`。
- 对外 public surface：`application/include/application/services/trace_diagnostics/LoggingServiceFactory.h` 与 `contracts/include/trace_diagnostics/contracts/*.h`。

## 结构归位

- `modules/trace-diagnostics/contracts/`：`M10` owner 专属追溯契约，以及 generic diagnostics sink contract。
- `shared/logging/`：跨模块复用日志基础设施（非 `M10` owner 实现）。

## 边界约束

- `docs/validation/` 仅承载验证说明，不承载追溯业务 owner 实现。
- `tests/` 与 `apps/trace-viewer/` 只消费 `M10` 事实，不承载 `M10` owner 实现。
- `hardware-test diagnostics contracts` 仍不属于 `M10`：`ITestRecordRepository` / `ITestConfigurationPort` / `ICMPTestPresetPort` / `TestDataTypes` 不在本模块落地。

## 当前协作面

- `docs/validation/`
- `apps/trace-viewer/`
- `shared/logging/`

## 统一骨架状态

- 已补齐 module.yaml、domain、services、application、adapters、tests 与 examples 子目录。
- `adapters/logging/spdlog/` 是当前明确的模块内部日志适配实现承载面。
- 对外 canonical public surface 固定为 `application/contracts`；adapter 不对外暴露。
- 终态实现必须继续落在该模块 canonical 骨架内，不再引入 bridge 目录。

## 当前测试面

- `tests/contract/`
  - 冻结最小 evidence bundle contract
- `tests/evidence/`
  - 冻结 `summary/timeline/artifacts/report_path` baseline 样本
- `tests/observability/`
  - 冻结 `CreateLoggingService` bootstrap 与日志可观测回归
