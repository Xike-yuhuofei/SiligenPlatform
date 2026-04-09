# Machine Residual Exit Note

`modules/coordinate-alignment/domain/machine/` 已退出 live build，不再承载任何 `M5 coordinate-alignment` 的 load-bearing residual。

## 已关闭项

- `CalibrationProcess` / `ICalibrationDevicePort` / `ICalibrationResultPort`
- `IHardwareConnectionPort`
- `IHardwareTestPort`
- workflow `domain/include/domain/machine/**` 对上述对象的 bridge wrappers

## 当前约束

- 不允许重新引入 machine/runtime/calibration legacy surface。
- 不允许把本目录重新接回 `siligen_module_coordinate_alignment` root surface。
- 若未来需要新能力，只能围绕 `CoordinateTransformSet` 或其正式 consumer seam 建模。
