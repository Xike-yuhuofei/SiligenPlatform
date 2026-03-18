# 应用容器 (Application Container)

> 说明：本目录已降级为 legacy 文档壳。真实容器实现位于
> `D:\Projects\SiligenSuite\packages\runtime-host\src\container\`。
> 新增系统级用例和领域端口请落到 `D:\Projects\SiligenSuite\packages\process-runtime-core\src\application` /
> `D:\Projects\SiligenSuite\packages\process-runtime-core\src\domain`，不要继续扩写 `control-core/src/application` /
> `control-core/src/domain`。

## 概述

ApplicationContainer是依赖注入容器,负责管理整个应用的依赖关系。它配置端口到适配器的绑定,并创建用例实例。

## 设计原则

### 1. 控制反转 (Inversion of Control)
- 组件不创建依赖,而是接收依赖
- 容器负责创建和组装所有组件

### 2. 依赖注入 (Dependency Injection)
- 构造函数注入(推荐)
- 接口依赖而非具体实现

### 3. 单一职责
- 容器只负责对象创建和组装
- 不包含业务逻辑

## 容器架构

```
ApplicationContainer
│
├── 端口绑定 (Port Bindings)
│   ├── IConfigurationPort → ConfigFileAdapter
│   ├── IHardwareConnectionPort → HardwareConnectionAdapter
│   ├── IMotionConnectionPort / IAxisControlPort / IPositionControlPort / IMotionStatePort / IJogControlPort / IIOControlPort → MultiCardMotionAdapter
│   ├── IHomingPort → HomingPortAdapter
│   ├── ITriggerControllerPort → TriggerControllerAdapter
│   ├── IDiagnosticsPort → DiagnosticsPortAdapter
│   └── IEventPublisherPort → EventPublisherAdapter
│
└── 用例创建 (Use Case Creation)
    ├── System::InitializeSystemUseCase
    ├── Motion::Homing::HomeAxesUseCase
    └── Motion::Trajectory::ExecuteTrajectoryUseCase
```

## 使用方法

### 基本使用

```cpp
#include "container/ApplicationContainer.h"

using namespace Siligen::Application;

int main() {
    // 1. 创建容器
    Container::ApplicationContainer container("config/machine_config.ini");

    // 2. 配置依赖
    container.Configure();

    // 3. 解析用例
    auto initialize_usecase = container.Resolve<UseCases::System::InitializeSystemUseCase>();

    // 4. 使用用例
    UseCases::System::InitializeSystemRequest request;
    request.load_configuration = true;
    request.auto_connect_hardware = true;
    request.start_heartbeat = true;

    auto result = initialize_usecase->Execute(request);

    if (result.IsSuccess()) {
        std::cout << "系统初始化成功" << std::endl;
    }

    return 0;
}
```

### 解析多个用例

```cpp
// 创建和配置容器
Container::ApplicationContainer container;
container.Configure();

// 解析不同的用例
auto init_usecase = container.Resolve<UseCases::System::InitializeSystemUseCase>();
auto home_usecase = container.Resolve<UseCases::Motion::Homing::HomeAxesUseCase>();
auto trajectory_usecase = container.Resolve<UseCases::Motion::Trajectory::ExecuteTrajectoryUseCase>();

// 执行业务流程
init_usecase->Execute(init_request);
home_usecase->Execute(home_request);
trajectory_usecase->Execute(trajectory_request);
```

### 直接访问端口

```cpp
// 如果需要直接使用端口(不推荐,应该通过用例)
auto motion_port = container.ResolvePort<Domain::Motion::Ports::IPositionControlPort>();

if (motion_port) {
    auto result = motion_port->MoveToPosition(Point2D(100, 100), 50.0f);
}
```

## 配置流程

### 1. 端口绑定

```cpp
void ApplicationContainer::ConfigurePorts() {
    // 创建硬件接口
    hardware_interface_ = std::make_shared<MultiCardInterface>();

    // 配置适配器
    auto config_adapter = std::make_shared<ConfigFileAdapter>(config_file_path_);
    config_port_ = config_adapter;
    RegisterPort<IConfigurationPort>(config_adapter);

    auto motion_adapter_result = MultiCardMotionAdapter::Create(hardware_interface_);
    if (motion_adapter_result.IsError()) {
        throw std::runtime_error(motion_adapter_result.GetError().ToString());
    }
    auto motion_adapter = motion_adapter_result.Value();
    RegisterPort<IMotionConnectionPort>(motion_adapter);
    RegisterPort<IAxisControlPort>(motion_adapter);
    RegisterPort<IPositionControlPort>(motion_adapter);
    RegisterPort<IMotionStatePort>(motion_adapter);
    RegisterPort<IJogControlPort>(motion_adapter);
    RegisterPort<IIOControlPort>(motion_adapter);

    // ... 其他适配器
}
```

### 2. 用例创建

```cpp
template<>
std::shared_ptr<InitializeSystemUseCase>
ApplicationContainer::CreateInstance<InitializeSystemUseCase>() {
    auto home_axes_usecase = Resolve<HomeAxesUseCase>();
    return std::make_shared<InitializeSystemUseCase>(
        config_port_,
        hardware_connection_port_,
        home_axes_usecase,
        diagnostics_port_,
        event_port_
    );
}
```

## 生命周期管理

### 单例模式
- 默认情况下,所有服务都是单例
- 第一次Resolve时创建,后续返回同一实例

```cpp
auto usecase1 = container.Resolve<InitializeSystemUseCase>();
auto usecase2 = container.Resolve<InitializeSystemUseCase>();
// usecase1 和 usecase2 指向同一实例
```

### 预注册实例

```cpp
// 预先创建并注册实例
auto custom_instance = std::make_shared<CustomService>();
container.RegisterSingleton<CustomService>(custom_instance);

// 后续Resolve返回预注册的实例
auto resolved = container.Resolve<CustomService>();
// resolved == custom_instance
```

## 添加新服务

### 步骤

1. **创建端口接口**
```cpp
// packages/process-runtime-core/src/domain/<subdomain>/ports/INewPort.h
// 若为系统级跨子域端口，放在 packages/process-runtime-core/src/domain/system/ports/
class INewPort {
public:
    virtual ~INewPort() = default;
    virtual Result<void> DoSomething() = 0;
};
```

2. **创建适配器实现**
```cpp
// src/infrastructure/adapters/NewAdapter.h
class NewAdapter : public INewPort {
public:
    Result<void> DoSomething() override {
        // 实现
    }
};
```

3. **在容器中注册**
```cpp
void ApplicationContainer::ConfigurePorts() {
    // ... 现有绑定

    // 添加新绑定
    auto new_adapter = std::make_shared<NewAdapter>();
    new_port_ = new_adapter;
    RegisterPort<INewPort>(new_adapter);
}
```

4. **创建使用新端口的用例**
```cpp
class NewUseCase {
public:
    explicit NewUseCase(std::shared_ptr<INewPort> new_port)
        : new_port_(new_port) {}

private:
    std::shared_ptr<INewPort> new_port_;
};
```

5. **添加用例创建模板特化**
```cpp
template<>
std::shared_ptr<NewUseCase>
ApplicationContainer::CreateInstance<NewUseCase>() {
    return std::make_shared<NewUseCase>(new_port_);
}
```

## 测试支持

### Mock依赖注入

```cpp
// 测试时使用Mock实现
class MockPositionControl : public IPositionControlPort {
public:
    Result<void> MoveToPosition(const Point2D& pos, float32 vel) override {
        // Mock实现
        move_called_ = true;
        last_position_ = pos;
        return Result<void>();
    }

    bool move_called_ = false;
    Point2D last_position_;
};

// 测试代码
TEST(UseCaseTest, MoveToPosition) {
    // 创建容器
    ApplicationContainer container;

    // 替换为Mock实现
    auto mock_motion = std::make_shared<MockPositionControl>();
    container.RegisterPort<IPositionControlPort>(mock_motion);

    // 配置其他依赖
    container.Configure();

    // 测试
    auto usecase = container.Resolve<UseCases::Motion::PTP::MoveToPositionUseCase>();
    usecase->Execute(request);

    EXPECT_TRUE(mock_motion->move_called_);
}
```

## 最佳实践

### 1. 依赖最小化
- 用例只依赖需要的端口
- 避免"上帝对象"依赖

### 2. 接口隔离
- 端口接口保持精简
- 一个端口一个职责

### 3. 构造函数注入
- 优先使用构造函数注入
- 所有依赖在构造时提供
- 避免setter注入

### 4. 显式依赖
- 不要在容器中隐藏依赖关系
- 依赖关系清晰可见

### 5. 单次配置
- Configure()只调用一次
- 在应用启动时完成所有配置

## 性能考虑

### 对象创建成本
- 使用单例模式减少创建开销
- 考虑对象池(如需要)

### 解析性能
- 首次解析会创建实例
- 后续解析直接返回缓存实例
- 时间复杂度: O(1) 哈希表查找

### 内存占用
- 单例实例在容器生命周期内保持
- 考虑延迟创建策略
- 不需要的服务可以不预创建

## 相关文档

- 用例文档: `packages/process-runtime-core/src/application/usecases/README.md`
- 端口定义: `packages/process-runtime-core/src/domain/<subdomain>/ports/`
- 适配器实现: `src/infrastructure/adapters/README.md`
