# 点胶最佳实践参数化方案（轻量版）

**日期**: 2026-01-21  
**目标**: 将行业最佳实践涉及的工艺控制点落入配置与校验，不改算法与执行流程  
**范围**: 配置模型、INI 读写、参数校验、默认配置说明

## 背景

行业最佳实践强调时间/压力、材料流变性、温度稳定、气源质量、回吸防滴、点径反馈等关键控制点。
当前系统已具备基础点胶参数，但缺少对上述工艺控制点的显式配置与校验。

## 方案概述

以“可配置 + 软校验”为主：
1) 扩展点胶配置模型，新增工艺控制字段；默认不启用，保持现有行为。  
2) 让配置文件真实生效（在保留硬编码兜底的同时读取 INI）。  
3) 对新增字段做范围与逻辑校验，提供明确错误信息。

## 变更范围（文件清单）

1. `src/domain/configuration/ports/IConfigurationPort.h`  
   - 扩展 `DispensingConfig`，新增工艺控制字段（示例）：
     - 温度/黏度目标与容差：`temperature_target_c`、`temperature_tolerance_c`、`viscosity_target_cps`、`viscosity_tolerance_pct`
     - 气源要求：`air_dry_required`、`air_oil_free_required`
     - 回吸与防滴：`suck_back_enabled`、`suck_back_ms`
     - 过程监控：`vision_feedback_enabled`、`dot_diameter_target_mm`、`dot_diameter_tolerance_mm`
     - 料筒液位：`syringe_level_min_pct`
   - 默认值保持“不开启/0”，避免影响现有流程。

2. `src/infrastructure/adapters/system/configuration/ConfigFileAdapter.h`  
   - 声明新增字段的加载、保存、校验逻辑。
   - 准备引入 INI 真实读写实现（参考 `IniTestConfigurationAdapter` 的 WinAPI 方式）。

3. `src/infrastructure/adapters/system/configuration/ConfigFileAdapter.cpp`  
   - `ReadIniValue` 改为真实读取 INI：优先返回硬编码固定值，否则读取 INI，再回退默认值。
   - `LoadDispensingSection`/`SaveDispensingSection` 增加新增字段读写。
   - `ValidateDispensingConfig` 增加范围/互斥校验（如容差范围、百分比范围、回吸启用时长限制等）。

4. `src/infrastructure/resources/config/machine_config.ini`  
   - 在 `[Dispensing]` 或新增 `[ProcessControl]` 段写入新增参数与说明注释。
   - 同步 `src/infrastructure/resources/config/files/machine_config.ini`，保持两份配置一致。

## 详细变更说明

### 1) 配置模型扩展
- 在 `DispensingConfig` 增加字段，覆盖最佳实践中的可控点：温度/黏度、气源质量、回吸、防滴、点径反馈、料筒液位。
- 所有新增字段默认不启用或为 0，确保向后兼容。

### 2) INI 读取策略
- 继续保留 `kFixedIniValues` 的硬编码兜底策略（用于兼容现有固定配置）。
- 当 `kFixedIniValues` 未覆盖时，使用 WinAPI `GetPrivateProfileStringA` 读取 INI 实际值。

### 3) 参数校验策略
- 新增字段按业务合理范围校验，并在 `GetValidationErrors` 中输出清晰错误信息。
- 不在执行层阻断流程，只在配置校验阶段给出提示。

## 验证方式

1. 修改 `machine_config.ini` 中新增字段，调用 `ConfigFileAdapter::LoadConfiguration()` 后检查 `DispensingConfig` 是否正确反映配置值。  
2. 构造越界参数，确认 `ValidateConfiguration()` 返回 false 且错误信息可读。  

## 风险与回滚

- 风险：INI 读取改为真实读取可能暴露配置缺失问题。  
  缓解：固定值优先 + 默认值回退，避免系统启动失败。  
- 回滚：恢复 `ReadIniValue` 仅使用固定值的逻辑即可。

