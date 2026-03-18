# 基础设施适配器 (Infrastructure Adapters)

## 概述

适配器(Adapters)实现了领域层定义的端口(Ports)接口,是六边形架构中连接领域层和外部世界的桥梁。

## 设计原则

### 1. 依赖反转
```
领域层 (Ports)  ← 定义接口
    ↑
    | 实现
    |
基础设施层 (Adapters)  ← 实现接口
```

### 2. 单一职责
- 每个适配器专注于一种外部系统的集成
- 适配器只做转换工作,不包含业务逻辑
  - 插补适配器仅负责硬件 API 调用与数据/单位转换，策略与校验由 Domain 统一入口提供

### 3. 技术无关性
- 领域层不知道适配器的存在
- 替换适配器不影响业务逻辑

## 适配器分类

### motion/controller - 运动控制适配器
**MultiCardMotionAdapter**
- 实现: `IMotionConnectionPort` / `IAxisControlPort` / `IPositionControlPort` / `IMotionStatePort` / `IJogControlPort` / `IIOControlPort`
- 职责: 封装MultiCard运动控制卡API
- 功能:
  - 轴位置控制
  - 速度和加速度设置
  - 状态监控
  - 紧急停止

### system/configuration - 配置适配器
**ConfigFileAdapter**
- 实现: `IConfigurationPort`
- 职责: 管理INI配置文件
- 功能:
  - 加载系统配置
  - 保存配置更改
  - 配置验证
  - 备份和恢复

## 使用示例

### 创建和使用适配器

```cpp
#include "infrastructure/adapters/motion/controller/control/MultiCardMotionAdapter.h"
#include "infrastructure/adapters/system/configuration/ConfigFileAdapter.h"

using namespace Siligen::Infrastructure::Adapters;

// 创建硬件接口
auto hardware = std::make_shared<MultiCardInterface>();

// 创建适配器
auto motion_adapter_result = MultiCardMotionAdapter::Create(hardware);
if (motion_adapter_result.IsError()) {
    std::cout << "创建失败: " << motion_adapter_result.GetError().ToString() << std::endl;
    return;
}
auto motion_adapter = motion_adapter_result.Value();
auto config_adapter = std::make_shared<ConfigFileAdapter>("config.ini");

// 通过端口接口使用
Domain::Motion::Ports::IPositionControlPort* motion_port = motion_adapter.get();
auto result = motion_port->MoveToPosition(Point2D(100, 100), 50.0f);

if (result.IsSuccess()) {
    std::cout << "运动成功" << std::endl;
} else {
    std::cout << "错误: " << result.GetError().ToString() << std::endl;
}
```

### 依赖注入

```cpp
// 在应用启动时配置依赖
class ApplicationContainer {
public:
    void Configure() {
        // 注册适配器
        RegisterSingleton<IPositionControlPort>([]() {
            auto result = MultiCardMotionAdapter::Create(...);
            if (result.IsError()) {
                throw std::runtime_error(result.GetError().ToString());
            }
            return result.Value();
        });

        RegisterSingleton<IConfigurationPort>(
            []() { return std::make_shared<ConfigFileAdapter>("config.ini"); }
        );
    }
};
```

## 添加新适配器

### 步骤

1. **在领域层创建端口接口**
   ```cpp
   // src/domain/<subdomain>/ports/INewPort.h
   // 若为系统级跨子域端口，放在 src/domain/system/ports/
   class INewPort {
   public:
       virtual ~INewPort() = default;
       virtual Result<void> DoSomething() = 0;
   };
   ```

2. **创建适配器实现**
   ```cpp
   // src/infrastructure/adapters/<category>/NewAdapter.h
   class NewAdapter : public Domain::Motion::Ports::INewPort {
   public:
       Result<void> DoSomething() override {
           // 调用外部系统API
           // 转换数据格式
           // 处理错误
           return Result<void>();
       }
   };
   ```

3. **编写单元测试**
   ```cpp
   // tests/adapters/test_new_adapter.cpp
   TEST(NewAdapterTest, DoSomething) {
       NewAdapter adapter;
       auto result = adapter.DoSomething();
       EXPECT_TRUE(result.IsSuccess());
   }
   ```

## 适配器实现规范

### 错误处理
- 所有可能失败的操作返回 `Result<T>`
- 捕获外部系统异常,转换为领域错误
- 提供清晰的错误消息

### 日志记录
- 记录关键操作
- 记录错误和异常
- 使用统一的日志格式

### 资源管理
- 正确管理外部资源(文件,连接等)
- 使用RAII模式
- 在析构函数中清理资源

### 线程安全
- 考虑并发访问场景
- 使用互斥锁保护共享状态
- 文档说明线程安全保证

## 测试策略

### 单元测试
- 测试每个适配器方法
- 使用Mock对象模拟外部系统
- 测试错误处理路径

### 集成测试
- 使用真实外部系统
- 测试完整工作流
- 验证数据转换正确性

## 维护和清理

### 死代码清理

本目录会定期进行死代码分析和清理。详细信息请参阅:

- **清理分析报告**: `CLEANUP_ANALYSIS_REPORT.md`
- **执行检查清单**: `CLEANUP_EXECUTION_CHECKLIST.md`
- **清理脚本**:
  - Windows: `cleanup-adapters.ps1`
  - Linux/WSL: `cleanup-adapters.sh`

### 清理原则

1. **零引用原则**: 如果适配器未被任何代码引用，标记为死代码
2. **优先级分级**: 按风险等级分 P0-P3 优先级清理
3. **验证驱动**: 每次清理后立即验证构建和测试
4. **可回滚**: 使用 Git 分支和标签支持快速回滚

### 当前适配器清单

#### 运动控制 (motion/controller/)
- ✅ `MultiCardMotionAdapter` - MultiCard 运动控制卡封装
- ✅ `HomingPortAdapter` - 回零操作适配器
- ✅ `InterpolationAdapter` - 坐标系插补（仅硬件调用，规则由 Domain 统一）
- ✅ `HardwareConnectionAdapter` - 硬件连接管理
- ✅ `UnitConverter` - 单位转换工具

#### 点胶控制 (dispensing/dispenser/)
- ✅ `ValveAdapter` - 点胶阀和供胶阀控制
- ✅ `TriggerControllerAdapter` - 位置触发控制

#### 配置管理 (system/configuration/)
- ✅ `ConfigFileAdapter` - INI 配置文件适配器
- ✅ `IniTestConfigManager` - 测试配置管理器
- ✅ `ConfigValidator` - 配置验证器

#### 诊断 (diagnostics/health/)
- ✅ `DiagnosticsPortAdapter` - 系统诊断与健康检查
- ✅ `CMPTestPresetService` - CMP 测试预设服务
- ✅ `EnumConverters` - 枚举转换器
- ✅ `JSONUtils` - JSON 工具类
- ✅ `HardwareTestAdapter` - 硬件测试功能 (health/testing)

#### 规划 (planning/)
- ✅ `AutoPathSourceAdapter` - 路径源（仅 PB）
- ✅ `PbPathSourceAdapter` - PB 路径读取

#### 可视化 (planning/visualization/)

#### 日志 (diagnostics/logging/)
- ✅ `SpdlogLoggingAdapter` - 日志服务
- ✅ `logging/MemorySink` - 内存日志 Sink

#### 系统运行时兼容入口 (system/runtime/)
- ✅ `events/InMemoryEventPublisherAdapter` - 兼容入口，主实现位于 `packages/runtime-host/src/runtime/events/`
- ✅ `scheduling/TaskSchedulerAdapter` - 兼容入口，主实现位于 `packages/runtime-host/src/runtime/scheduling/`

#### 存储 (system/storage/)
- ✅ `LocalFileStorageAdapter` - 本地文件存储

#### 持久化 (system/persistence/)
- ✅ `MockTestRecordRepository` - 内存测试记录仓储

#### 已清理（2026-01-18）

以下文件/目录已在清理中删除:

- ❌ `logging/SpdlogLoggingAdapter` - 重复文件（移至 diagnostics/logging）
- ❌ `external/` - 空目录
- ❌ `system/clock/SystemClockAdapter` - 未使用
- ❌ `system/env/EnvironmentInfoAdapter` - 未使用
- ❌ `system/stacktrace/BoostStacktraceAdapter` - 未使用
- ❌ `diagnostics/logging/ConsoleLogger` - 未使用
- ❌ `diagnostics/logging/UILogProvider` - 未使用
- ❌ `system/configuration/INIConfigManager` - 未使用
- ❌ `system/configuration/validators/HardwareValidator` - 未使用
- ❌ `system/storage/bugstore/FileBugStoreAdapter` - 未使用
- ❌ `diagnostics/health/serializers/*TestDataSerializer` - 未使用的序列化器（6个文件）
- ❌ `application/parsers/` - 重复目录（已在 planning/dxf/internal/）

## 相关文档

- 领域端口: `src/domain/<subdomain>/ports/`
- 六边形架构: `docs/library/02_architecture.md`
- 架构决策: `docs/adr/ADR-0001-hexagonal-architecture.md`
- 清理分析: `CLEANUP_ANALYSIS_REPORT.md`
- 执行清单: `CLEANUP_EXECUTION_CHECKLIST.md`

