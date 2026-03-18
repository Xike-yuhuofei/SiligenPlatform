# DXF 预览历史资产归档

本目录归档的是已从 `control-core` 剥离的 DXF 预览历史实现，归档日期为 2026-03-16。

## 归档原因
- DXF 预览生成职责已迁移到 `D:\Projects\SiligenSuite\dxf-pipeline`
- DXF 预览展示职责已迁移到 `D:\Projects\SiligenSuite\hmi-client`
- `control-core` 仅保留系统初始化、运动控制、点胶执行、硬件适配、配方执行、运行时调度六类核心职责

## 本次归档对象
- `src/domain/planning/ports/IVisualizationPort.h`
- `src/infrastructure/adapters/planning/visualization/dispensing/DispensingVisualizer.*`
- `src/infrastructure/adapters/planning/visualization/dxf/DXFVisualizationAdapter.*`

## 说明
这些文件保留为历史参考资产，不再参与 `control-core` 编译与运行时装配。
