# coordinate-alignment contracts

`modules/coordinate-alignment/contracts/` 承载 `M5 coordinate-alignment` 的模块 owner 专属契约。

## 契约范围

- 坐标对齐输入/输出对象的模块内字段口径
- 对齐参数、变换规则与结果语义约束
- `M5 -> M6` 交接所需的最小契约面

当前事实补充：

- `CoordinateTransformSet` 是当前唯一可回到 `S05` owner 语义的 `M5` seam。
- `IHardwareTestPort.h` 仍是 root target 暂挂的 consumer residual public header，当前被 runtime-execution device adapter 依赖。
- 因此 `contracts/` 目前是模块 root surface，但还不是纯粹的 `M5` owner-only contract surface。

## 边界约束

- 仅放置 `coordinate-alignment` owner 专属契约，不放跨模块稳定公共契约。
- 跨模块长期稳定工程契约应维护在 `shared/contracts/engineering/`。
- `modules/coordinate-alignment/` 与 `shared/contracts/engineering/` 构成当前 canonical 契约承载面。
- 若要移除或迁移 `IHardwareTestPort.h`，必须同步完成 runtime-execution consumer 清理；该动作超出本轮低成本整理边界。
