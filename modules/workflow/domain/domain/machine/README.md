# Machine 兼容壳

`modules/workflow/domain/domain/machine/` 不再持有 machine aggregate / calibration flow 的 live 实现。

## 当前 owner 边界

- `DispenserModel` concrete owner 固定在 `modules/runtime-execution/application/system/LegacyDispenserModel.*`
- `CalibrationWorkflowService` concrete owner 固定在 `modules/runtime-execution/application/services/calibration/`
- `CalibrationExecutionTypes`、`ICalibrationExecutionPort`、`ICalibrationResultSink` owner 固定在 `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/system/`
- workflow 不再为 machine/calibration 暴露 compatibility header 或 compatibility target

## 兼容规则

- `Aggregates::Legacy::DispenserModel` 的 workflow shim 已删除，不允许恢复
- `DomainServices::CalibrationProcess` 的 workflow alias 已删除，不允许恢复
- `CalibrationTypes.h`、`ICalibrationDevicePort.h`、`ICalibrationResultPort.h` 的 workflow alias 已删除，不允许恢复
- 禁止在本目录重新新增 live `.cpp`

## 构建约束

- `modules/workflow/domain/domain/machine/CMakeLists.txt` 不再定义 `domain_machine`
- 缺少 canonical owner target 时必须显式失败，禁止回退到 workflow 本地实现
