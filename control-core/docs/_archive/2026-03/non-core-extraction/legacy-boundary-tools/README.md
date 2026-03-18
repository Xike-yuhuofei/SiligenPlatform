# 兼容边界遗留资产归档

本目录归档的是已确认不再服务于 `control-core` 六类核心职责的兼容边界代码，归档日期为 2026-03-16。

## 已归档对象
- `src/shared/interfaces/IConfigurationService.h`
- `src/shared/factories/AdapterFactory.h`
- `src/infrastructure/CMakeLists.txt.patch`

## 归档判断
- 上述接口、工厂与补丁文件不再参与当前组合根、运行时调度、运动控制或点胶执行主链
- 保留为历史参考，不再保留在 `control-core/src` 活跃代码树内
