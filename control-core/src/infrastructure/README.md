# Infrastructure Layer (基础设施层)

## 目录

本层包含技术实现、外部系统集成和适配器实现。

### adapters/
适配器实现,按业务流程分组:
- `motion/controller/` - 连接、运动控制、回零、插补（插补适配器仅硬件调用）
- `motion/monitoring/` - 运行态监测（预留）
- `motion/safety/` - 安全互锁（预留）
- `dispensing/dispenser/` - 点胶阀与触发控制
- `planning/dxf/` - DXF解析与轨迹生成
- `planning/visualization/` - DXF与点胶可视化
- `diagnostics/health/` - 诊断、序列化与测试预设
- `diagnostics/logging/` - 日志适配器
- `system/configuration/` - 配置读取与校验
- `system/persistence/` - 测试记录持久化
- `system/storage/` - 文件存储/缺陷记录
- `system/runtime/` - 事件发布与任务调度器

### drivers/
驱动与外部库封装:
- `multicard/` - MultiCard SDK 封装与包装器

### resources/
配置与资源文件:
- `config/` - INI配置
- `schema/` - 数据库脚本

## 依赖原则

- **依赖** domain/<subdomain>/ports 与 shared
- **实现** domain 层定义的端口接口
- **不依赖** application/usecases 或其他 adapters 实现
- **插补边界**: 策略/校验/程序生成在 Domain，基础设施仅做 API 调用、单位转换与错误映射

## 命名空间

```cpp
namespace Siligen::Infrastructure {
    namespace Adapters { }  // 适配器实现
    namespace Drivers { }   // 驱动封装
}
```
