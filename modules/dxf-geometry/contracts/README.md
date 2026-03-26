# dxf-geometry/contracts

`modules/dxf-geometry/contracts/` 是 `M2 dxf-geometry` 的模块契约入口。

## 正式职责

- 记录 DXF 几何解析相关的模块内契约与输入引用规则。
- 承接仅由 `M2 dxf-geometry` owner 的契约说明，支撑上游静态工程链收敛。
- 作为 `modules/dxf-geometry/` 的契约锚点，供 `Wave 2` cutover 与门禁校验引用。

## 不应承载

- 跨模块稳定公共契约（应放入 `shared/contracts/engineering/`）。
- 运行时执行、设备适配或 HMI 展示层语义。
- 未声明迁移路径与退出条件的 legacy 回写说明。

## 当前事实来源

- `modules/dxf-geometry/application/engineering_data/`
- `shared/contracts/engineering/`
