# 依赖注入容器使用指南

> **项目**: Siligen工业点胶机控制系统
> **版本**: 2.0 (统一后)
> **日期**: 2026-01-08

---

## 概述

项目采用**分层DI策略**，使用两个核心容器：

| 容器 | 用途 | 适用场景 |
|------|------|----------|
| **ApplicationContainer** | 主DI容器（运行时） | 配置管理、文件IO、UI、所有用例 |
| **ZeroOverheadDI** | 零开销DI（编译时） | 实时运动控制、插补、高频缓冲区管理 |
| **TestContainer** | 测试专用 | 单元测试、集成测试（Mock支持） |

---

## ApplicationContainer使用指南

### 1. 基本用法

```cpp
#include "application/container/ApplicationContainer.h"

using namespace Siligen::Application::Container;

// 创建容器
ApplicationContainer container("config/machine_config.ini");

// 配置所有依赖（必须在使用前调用）
container.Configure();

// 解析用例
auto initializeUseCase = container.Resolve<UseCases::InitializeSystemUseCase>();
auto result = initializeUseCase->Execute(request);

// 解析端口
auto motionPort = container.ResolvePort<Domain::Ports::IPositionControlPort>();
motionPort->MoveToPosition(axis, position);
```

### 2. Composition Root模式

**在适配器层使用Composition Root**（推荐）:

```cpp
// src/adapters/cli/CLICompositionRoot.h
class CLICompositionRoot {
public:
    explicit CLICompositionRoot(const std::string& config_path)
        : container_(config_path) {
        container_.Configure();
    }

    template<typename TUseCase>
    std::shared_ptr<TUseCase> ResolveUseCase() {
        return container_.Resolve<TUseCase>();
    }

private:
    ApplicationContainer container_;
};

// main.cpp
int main() {
    CLICompositionRoot root("config/machine_config.ini");
    auto useCase = root.ResolveUseCase<InitializeSystemUseCase>();
    useCase->Execute();
}
```

### 3. 端口注册

端口在`ApplicationContainer::ConfigurePorts()`中自动注册，无需手动注册。

```cpp
// ApplicationContainer.cpp内部实现
void ApplicationContainer::ConfigurePorts() {
    // 创建MultiCard实例
    multiCard_ = CreateMultiCardInstance(config_file_path_);

    // 创建基础设施工厂
    infrastructure_factory_ = std::make_shared<InfrastructureAdapterFactory>(multiCard_);

    // 注册端口 -> 适配器绑定
    config_port_ = infrastructure_factory_->CreateConfigurationPort();
    motion_port_ = infrastructure_factory_->CreateMotionPort();
    // ... 其他端口
}
```

### 4. 用例解析

用例在`ApplicationContainer::ConfigureUseCases()`中注册，通过模板特化创建实例。

```cpp
// 自动注入依赖
auto useCase = container.Resolve<UseCases::InitializeSystemUseCase>();
// 等价于:
// auto useCase = std::make_shared<InitializeSystemUseCase>(
//     config_port_,
//     motion_port_,
//     homing_port_,
//     diagnostics_port_
// );
```

---

## TestContainer使用指南

### 1. 单元测试

```cpp
#include "../utils/TestServiceContainer.h"
#include "../mocks/MockHardwareController.h"

TEST(MyUseCase, ExecuteSuccess) {
    using namespace Siligen::Test::Utils;

    // 创建测试容器
    TestServiceContainer container;

    // 注册Mock对象
    auto mock_hardware = std::make_shared<MockHardwareController>();
    container.RegisterSingleton<IHardwareController>(mock_hardware);

    // 配置Mock行为
    EXPECT_CALL(*mock_hardware, Connect(_)).WillOnce(Return(Result<void>{}));

    // 解析服务
    auto hardware = container.Resolve<IHardwareController>();
    auto result = hardware->Connect(config);

    // 验证结果
    EXPECT_TRUE(result.IsSuccess());
}
```

### 2. 测试夹具

```cpp
#include "../utils/TestServiceContainer.h"

class MyUseCaseTest : public ServiceContainerTestFixture {
protected:
    void OnSetUpContainer() override {
        // 配置测试容器
        auto mock_hardware = std::make_shared<MockHardwareController>();
        GetContainer().RegisterSingleton<IHardwareController>(mock_hardware);
    }
};

TEST_F(MyUseCaseTest, ExecuteSuccess) {
    // 使用夹具提供的容器
    auto hardware = Resolve<IHardwareController>();
    // ... 测试逻辑
}
```

---

## ZeroOverheadDI使用指南

### 1. 适用场景

仅用于**实时控制路径**（< 1ms执行时间）:

```cpp
// src/domain/motion/RealtimeMotionContainer.h
#include "shared/di/ZeroOverheadDI.h"

// 定义实时容器类型
using RealtimeMotionContainer = ZeroOverheadContainer<
    InterpolationService,
    BufferManager,
    TriggerCalculator
>;

// 编译时解析，零开销
constexpr RealtimeMotionContainer container;

// 获取服务（编译时检查，零运行时开销）
auto& interpolator = container.get<InterpolationService>();
auto& buffer = container.get<BufferManager>();
```

### 2. 使用原则

**✅ 可以使用ZeroOverheadDI**:
- 实时插补循环（>1KHz）
- 高频缓冲区管理
- 运动控制内循环

**❌ 不应该使用ZeroOverheadDI**:
- 配置管理（使用ApplicationContainer）
- 文件IO（使用ApplicationContainer）
- UI交互（使用ApplicationContainer）
- 测试代码（使用TestContainer）

---

## 常见问题

### Q1: 为什么不统一到一个容器？

**A:** 实时系统需要零开销抽象。ApplicationContainer有运行时开销（类型查找、哈希表），不适合实时路径。ZeroOverheadDI编译时解析，零运行时开销，但功能受限。

### Q2: 如何选择容器？

**决策树**:
```
是否是单元测试？
├─ 是 → TestContainer
└─ 否 → 是否是实时控制路径（<1ms）？
    ├─ 是 → ZeroOverheadDI
    └─ 否 → ApplicationContainer
```

### Q3: ServiceContainer去哪了？

**A:** ServiceContainer已被ApplicationContainer替代。功能完全兼容，请使用ApplicationContainer。

### Q4: 如何添加新的用例？

1. 在`src/application/usecases/`创建用例类
2. 在`ApplicationContainer.h`添加模板特化声明
3. 在`ApplicationContainer.cpp`实现特化
4. 在`ConfigureUseCases()`中不需要手动注册（自动通过CreateInstance）

示例:
```cpp
// ApplicationContainer.h
template<>
std::shared_ptr<UseCases::MyNewUseCase>
ApplicationContainer::CreateInstance<UseCases::MyNewUseCase>();

// ApplicationContainer.cpp
template<>
std::shared_ptr<UseCases::MyNewUseCase>
ApplicationContainer::CreateInstance<UseCases::MyNewUseCase>() {
    return std::make_shared<UseCases::MyNewUseCase>(
        dependency1_,
        dependency2_
    );
}
```

---

## 参考资料

- [依赖注入统一方案](../plans/2026-01-08-di-unification-migration.md)
- [ApplicationContainer实现](../../src/application/container/ApplicationContainer.h)
- [ZeroOverheadDI实现](../../src/shared/di/ZeroOverheadDI.h)
- [TestContainer实现](../../tests/utils/TestServiceContainer.h)

---

**创建日期**: 2026-01-08
**最后更新**: 2026-01-08
**维护者**: @lead-architect
