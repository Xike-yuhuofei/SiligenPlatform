# Domain Ports 子域下沉设计

**目标**  
清空 `src/domain/<subdomain>/ports`，将根端口按子域边界下沉，统一路径与命名空间，避免全局端口无限扩散。

**范围与归属**  
- `domain/system/ports/`: `IEventPublisherPort`  
- `domain/planning/ports/`: `IDXFFileParsingPort`、`IVisualizationPort`  
- `domain/motion/ports/`: `IPositionControlPort`、`MotionControllerPortBase`、`IIOControlPort`  
- `domain/dispensing/ports/`: `IValvePort`  
- `domain/diagnostics/ports/`: `ITestConfigManager`、`ITestRecordRepository`、`ICMPTestPresetPort`  
- `domain/configuration/ports/`: `ValveConfig.h`  
完成后删除空的 `src/domain/<subdomain>/ports`。

**迁移策略**  
1) 新建 `system/ports` 与 `planning/ports` 目录。  
2) 移动端口文件到对应子域路径。  
3) 批量替换 include 与命名空间：`Domain::Ports::*` → `Domain::<Subdomain>::Ports::*`。  
4) 更新 CMake 与 README/架构文档中端口路径示例。  
5) 全局扫 `domain/<subdomain>/ports/` 与 `Domain::Ports::` 确保无残留。  
6) 构建验证（`cmake --build ... --target siligen_control_application`）。

**兼容性与风险**  
不改接口语义与签名，仅移动与重命名命名空间。主要风险是遗漏引用与旧路径残留，靠全局扫描与编译错误定位修复。

**测试策略**  
执行主目标构建；如有失败，按错误逐点修复。若需更高信心，可补充关键用例或适配器的编译目标验证。


