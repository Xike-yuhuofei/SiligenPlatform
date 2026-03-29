# process-planning contracts

`modules/process-planning/contracts/` 承载 `M4 process-planning` 的模块 owner 专属契约。

## 契约范围

- `ConfigurationContracts.h` 暴露的 configuration-only consumer surface。
- 规划配置读取、默认值与兼容桥接所需的最小类型口径。
- 对 `legacy-bridge/include/domain/configuration/ports/IConfigurationPort.h` 的稳定转发，不扩张为完整 `ProcessPlan` 契约。

## 边界约束

- 本目录只声明 configuration owner 边界，不声明 `FeatureGraph -> ProcessPlan`、commands、events 或完整 `ProcessPlan` owner。
- 仅放置 `M4 process-planning` owner 专属契约，不放跨模块稳定公共契约。
- 不承载运行时执行层与 HMI 展示层语义。
- `modules/process-planning/` 与 `shared/contracts/engineering/` 构成当前 canonical 契约承载面。
