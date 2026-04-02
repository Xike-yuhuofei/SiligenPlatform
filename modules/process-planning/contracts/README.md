# process-planning contracts

`modules/process-planning/contracts/` 承载 `M4 process-planning` 的模块 owner 专属契约，也是当前 configuration 四件套的 physical owner root。

## 契约范围

- `ConfigurationContracts.h` 暴露的 configuration-only consumer surface。
- canonical config headers 固定为 `process_planning/contracts/configuration/{ConfigTypes,IConfigurationPort,ValveConfig,ReadyZeroSpeedResolver}.h`，真实定义直接落在该目录。
- 规划配置读取与默认值解析所需的最小类型口径。
- deprecated `legacy-bridge/include/domain/configuration/ports/IConfigurationPort.h` 已删除，禁止恢复 configuration legacy bridge target 或桥接头。
- `ConfigTypes.h` 的 `Position3D` 与 `InterlockConfig` 已在 `M4` canonical contract 内自持定义，不再依赖 `motion-planning` / `workflow` public include root。

## 边界约束

- 本目录只声明 configuration owner 边界，不声明 `FeatureGraph -> ProcessPlan`、commands、events 或完整 `ProcessPlan` owner。
- 仅放置 `M4 process-planning` owner 专属契约，不放跨模块稳定公共契约。
- 不承载运行时执行层与 HMI 展示层语义。
- `modules/process-planning/domain/configuration/**` 现阶段只保留 implementation `.cpp`，不再保留 deprecated wrapper 或 configuration public header owner。
- `modules/process-planning/` 与 `shared/contracts/engineering/` 构成当前 canonical 契约承载面。
