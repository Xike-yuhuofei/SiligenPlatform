# Machine Residual Exit Note

`modules/coordinate-alignment/domain/machine/` 已退出 live build，不再承载任何 `M5 coordinate-alignment` 的 load-bearing residual。

当前目录只保留本说明文档；dead forwarder header 已在本轮删除。

## 已关闭项

- `CalibrationProcess` / `ICalibrationDevicePort` / `ICalibrationResultPort`
- `IHardwareConnectionPort`
- `IHardwareTestPort`
- workflow `domain/include/domain/machine/**` 对上述对象的 bridge wrappers

## 当前约束

- 不允许重新引入 machine/runtime/calibration legacy surface。
- 不允许把本目录重新接回 `siligen_module_coordinate_alignment` root surface。
- 若未来需要新能力，只能围绕 `CoordinateTransformSet` 或其正式 consumer seam 建模。
- 若未来要清掉 `contracts/include/coordinate_alignment/contracts/IHardwareTestPort.h`，必须连同 runtime-execution consumer 一次性迁移；该动作超出本轮边界。
