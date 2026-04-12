# coordinate-alignment contracts

`modules/coordinate-alignment/contracts/` 承载 `M5 coordinate-alignment` 的模块 owner 专属契约。

## 契约范围

- 坐标对齐输入/输出对象的模块内字段口径
- 对齐参数、变换规则与结果语义约束
- `M5 -> M6` 交接所需的最小契约面
- 当前冻结 contract 仅保留 `CoordinateTransformSet`

当前事实补充：

- `CoordinateTransformSet` 是当前唯一可回到 `S05` owner 语义的 `M5` seam。
- `IHardwareTestPort` 已退出本目录；`contracts/include` 不再承载 machine-test consumer residual。
- 因此 `contracts/` 当前既是模块 root surface，也是收敛后的 `M5` owner contract surface。

## 边界约束

- 仅放置 `coordinate-alignment` owner 专属契约，不放跨模块稳定公共契约。
- 跨模块长期稳定工程契约应维护在 `shared/contracts/engineering/`。
- `modules/coordinate-alignment/` 与 `shared/contracts/engineering/` 构成当前 canonical 契约承载面。
- `process-path` 当前仅通过 `CoordinateTransformSet` 消费本模块 owner seam，不再依赖 machine-test residual。
