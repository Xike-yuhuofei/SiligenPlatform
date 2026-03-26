# motion-planning/contracts

`modules/motion-planning/contracts/` 是 `M7 motion-planning` 的模块契约入口。

## 正式职责

- 承载 `M7` 模块 owner 专属的运动规划契约说明。
- 定义规划输出对象与约束语义的模块边界。
- 作为 `Wave 3` owner path ready 的契约锚点。

## 不应承载

- 跨模块稳定公共契约（应归位到 `shared/contracts/` 对应域）。
- 运行时执行层设备适配与实时控制语义。
- 未声明迁移路径与退出条件的 legacy 双写说明。

## 当前事实来源

- `modules/motion-planning/domain/motion/`
- `shared/contracts/engineering/`
