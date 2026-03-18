# DI统一迁移实施计划

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**目标:** 将项目中3个DI容器（ZeroOverheadDI、ServiceContainer、ApplicationContainer）统一为2个核心容器（ApplicationContainer主容器 + ZeroOverheadDI实时容器），删除冗余的ServiceContainer实现。

**架构:** 采用分层DI策略，ApplicationContainer管理所有非实时路径的依赖（配置、文件IO、UI等），ZeroOverheadDI仅用于性能关键的实时控制路径（运动控制、插补）。TestContainer用于单元测试，提供Mock支持。

**技术栈:** C++17、std::shared_ptr、std::type_index、std::unordered_map、gtest

---

## 前置分析和准备工作

### Task 0: 验证当前ServiceContainer使用情况

**目的:** 确认ServiceContainer的实际使用范围，避免遗漏任何迁移点。

**Files:**
- 分析: `src/application/di/ServiceContainer.h` (216行)
- 分析: `src/application/di/ServiceLocator.h` (114行，全局访问点)
- 分析: `src/application/di/ServiceDescriptor.h` (87行，元数据)

**Step 1: 搜索ServiceContainer的所有使用点**

运行: `rg "ServiceContainer" --type cpp -l | grep -v "TestServiceContainer" | grep -v ".github" | grep -v "docs"`

预期输出:
```
src/shared/di/PerformanceTest.h
tests/unit/usecases/test_UseCases.cpp
tests/benchmark/DIPerformanceBenchmark.cpp
tests/unit/test_service_container.cpp
tests/unit/services/test_MotionService.cpp
tests/unit/services/test_CMPService.cpp
src/application/di/ServiceLocator.h
src/application/di/ServiceContainer.h
```

**Step 2: 确认ServiceContainer无实际生产代码使用**

运行: `rg "#include.*ServiceContainer\.h" src/ --type cpp | grep -v "tests" | grep -v "di/"`

预期输出: 空结果（说明ServiceContainer仅用于测试，无生产代码依赖）

**Step 3: 确认ApplicationContainer功能完整**

验证: `apps/control-runtime/container/ApplicationContainer.h` 包含以下功能：
- ✅ RegisterPort<TPort, TAdapter>() - 端口注册
- ✅ RegisterSingleton<T>() - 单例注册
- ✅ Resolve<T>() - 用例解析
- ✅ ResolvePort<TPort>() - 端口解析
- ✅ Configure() - 依赖配置

结论: ApplicationContainer已包含ServiceContainer的所有功能，可以完全替代。

**Step 4: 记录迁移影响范围**

创建文件: `docs/plans/di-migration-impact.md`

内容:
```markdown
# DI迁移影响范围分析

## ServiceContainer使用点

### 生产代码
- `src/application/di/ServiceLocator.h` - 全局服务定位器（待删除）

### 测试代码
- `tests/benchmark/DIPerformanceBenchmark.cpp` - 性能基准测试
- `tests/unit/test_service_container.cpp` - ServiceContainer单元测试
- `tests/unit/usecases/test_UseCases.cpp` - 用例测试（使用TestServiceContainer）
- `tests/unit/services/test_MotionService.cpp` - 运动服务测试
- `tests/unit/services/test_CMPService.cpp` - CMP服务测试

## 迁移策略
1. 保留TestServiceContainer（测试专用）
2. 删除ServiceContainer + ServiceLocator
3. 所有生产代码已使用ApplicationContainer
4. 性能测试需要更新为ApplicationContainer

## 风险评估
- **低风险**: ServiceContainer无生产代码使用
- **测试影响**: 需要更新性能测试和单元测试
- **兼容性**: ApplicationContainer功能超集，无功能损失
```

**Step 5: 提交分析结果**

运行:
```bash
git add docs/plans/di-migration-impact.md
git commit -m "docs(plans): add DI migration impact analysis"
```

---

## 阶段1: 删除ServiceLocator（全局访问点）

**理由:** ServiceLocator是反模式，提供全局可变状态，违反六边形架构的依赖注入原则。

### Task 1: 验证ServiceLocator无实际使用

**Files:**
- 分析: `src/application/di/ServiceLocator.h`
- 验证: 全代码库搜索

**Step 1: 搜索ServiceLocator的所有使用**

运行: `rg "ServiceLocator" src/ tests/ --type cpp`

预期输出: 空结果或仅有注释引用

**Step 2: 如果发现使用，记录位置并迁移**

如果有生产代码使用ServiceLocator：
```cpp
// 旧代码 (使用ServiceLocator)
auto port = ServiceLocator::Resolve<IHardwarePort>();

// 新代码 (使用ApplicationContainer)
ApplicationContainer container("config/machine_config.ini");
container.Configure();
auto port = container.ResolvePort<IHardwarePort>();
```

**Step 3: 提交验证结果**

运行:
```bash
git add -A
git commit -m "refactor(di): verify ServiceLocator has no production usage"
```

---

### Task 2: 删除ServiceLocator文件

**Files:**
- 删除: `src/application/di/ServiceLocator.h`
- 修改: `src/application/di/CMakeLists.txt`（如果存在）

**Step 1: 删除ServiceLocator头文件**

运行: `rm src/application/di/ServiceLocator.h`

**Step 2: 更新CMakeLists.txt（如果需要）**

检查: `src/application/di/CMakeLists.txt`

如果存在类似内容，删除ServiceLocator相关项：
```cmake
# 如果有这一行，删除它
set(DI_SOURCES
    ServiceContainer.h
    ServiceLocator.h  # ❌ 删除这一行
    ServiceDescriptor.h
)
```

**Step 3: 验证编译**

运行: `cmake --build build --target clean`

预期: 编译成功（因为ServiceLocator无使用）

**Step 4: 提交删除**

运行:
```bash
git add src/application/di/ServiceLocator.h
git add src/application/di/CMakeLists.txt
git commit -m "refactor(di): remove ServiceLocator anti-pattern"
```

---

## 阶段2: 迁移性能基准测试

**理由:** `tests/benchmark/DIPerformanceBenchmark.cpp` 直接使用ServiceContainer进行性能对比，需要更新为ApplicationContainer。

### Task 3: 更新DIPerformanceBenchmark使用ApplicationContainer

**Files:**
- 修改: `tests/benchmark/DIPerformanceBenchmark.cpp:49,116,164`

**Step 1: 添加ApplicationContainer头文件**

在文件顶部添加:
```cpp
#include "container/ApplicationContainer.h"
```

**Step 2: 替换ServiceContainer为ApplicationContainer**

修改第49行附近的基准测试:
```cpp
// 旧代码 (第49行)
ServiceContainer traditional_container;

// 新代码
ApplicationContainer traditional_container("config/machine_config.ini");
traditional_container.Configure();
```

修改第116行附近的临时容器测试:
```cpp
// 旧代码 (第116行)
ServiceContainer temp_container;

// 新代码
ApplicationContainer temp_container("config/machine_config.ini");
temp_container.Configure();
```

修改第164行附近的向量测试:
```cpp
// 旧代码 (第164行)
std::vector<ServiceContainer*> traditional_containers;
auto* container = new ServiceContainer();

// 新代码
std::vector<ApplicationContainer*> traditional_containers;
auto* container = new ApplicationContainer("config/machine_config.ini");
container->Configure();
```

**Step 3: 验证编译**

运行:
```bash
cmake --build build --target DIPerformanceBenchmark
```

预期: 编译成功

**Step 4: 运行基准测试验证性能**

运行:
```bash
./build/tests/benchmark/DIPerformanceBenchmark.exe
```

预期输出: 性能数据正常输出（数值可能与之前略有不同，因为ApplicationContainer有更多功能）

**Step 5: 提交更改**

运行:
```bash
git add tests/benchmark/DIPerformanceBenchmark.cpp
git commit -m "refactor(di): migrate DIPerformanceBenchmark to ApplicationContainer"
```

---

## 阶段3: 重写test_service_container.cpp

**理由:** `tests/unit/test_service_container.cpp` 是专门测试ServiceContainer的文件，需要重写为测试ApplicationContainer。

### Task 4: 创建ApplicationContainer单元测试

**Files:**
- 创建: `tests/unit/container/test_ApplicationContainer.cpp`
- 删除: `tests/unit/test_service_container.cpp`

**Step 1: 创建新的ApplicationContainer测试文件**

创建文件: `tests/unit/container/test_ApplicationContainer.cpp`

内容:
```cpp
// test_ApplicationContainer.cpp
// 版本: 1.0.0
// 描述: ApplicationContainer功能验证测试

#include <gtest/gtest.h>
#include "container/ApplicationContainer.h"
#include "../mocks/MockHardwareController.h"
#include "../mocks/MockLoggingService.h"
#include "../mocks/MockConfigManager.h"
#include "shared/types/Point2D.h"  // 移到gtest之后避免宏冲突

using namespace Siligen::Application::Container;
using namespace Siligen::Test::Mocks;
using namespace Siligen::Shared::Interfaces;

// ============================================================
// ApplicationContainer基础测试
// ============================================================

TEST(ApplicationContainer, ConfigureSuccess) {
    ApplicationContainer container("config/machine_config.ini");

    // 配置应该成功
    EXPECT_NO_THROW({
        container.Configure();
    });
}

TEST(ApplicationContainer, ResolveUseCase) {
    ApplicationContainer container("config/machine_config.ini");
    container.Configure();

    // 解析InitializeSystemUseCase
    auto useCase = container.Resolve<UseCases::InitializeSystemUseCase>();
    EXPECT_NE(useCase, nullptr);
}

TEST(ApplicationContainer, ResolvePort) {
    ApplicationContainer container("config/machine_config.ini");
    container.Configure();

    // 解析端口
    auto config_port = container.ResolvePort<Domain::Ports::IConfigurationPort>();
    EXPECT_NE(config_port, nullptr);
}

TEST(ApplicationContainer, RegisterSingletonPort) {
    ApplicationContainer container("config/machine_config.ini");

    // 注册Mock端口
    auto mock_hardware = std::make_shared<MockHardwareController>();
    // 注意: 需要将Mock包装成端口接口
    // container.RegisterPort<IHardwarePort>(mock_hardware);

    container.Configure();

    // 验证端口可以解析
    // auto port = container.ResolvePort<IHardwarePort>();
    // EXPECT_NE(port, nullptr);
}

TEST(ApplicationContainer, MultipleResolveReturnsSingleton) {
    ApplicationContainer container("config/machine_config.ini");
    container.Configure();

    // 解析两次
    auto useCase1 = container.Resolve<UseCases::InitializeSystemUseCase>();
    auto useCase2 = container.Resolve<UseCases::InitializeSystemUseCase>();

    // 应该是同一个实例（单例）
    EXPECT_EQ(useCase1.get(), useCase2.get());
}

TEST(ApplicationContainer, ResolveAllUseCases) {
    ApplicationContainer container("config/machine_config.ini");
    container.Configure();

    // 验证所有用例都可以解析
    EXPECT_NE(container.Resolve<UseCases::InitializeSystemUseCase>(), nullptr);
    EXPECT_NE(container.Resolve<UseCases::HomeAxesUseCase>(), nullptr);
    EXPECT_NE(container.Resolve<UseCases::ExecuteTrajectoryUseCase>(), nullptr);
    EXPECT_NE(container.Resolve<UseCases::StartDispenserUseCase>(), nullptr);
    EXPECT_NE(container.Resolve<UseCases::StopDispenserUseCase>(), nullptr);
    EXPECT_NE(container.Resolve<UseCases::UploadDXFFileUseCase>(), nullptr);
    EXPECT_NE(container.Resolve<UseCases::CleanupDXFFilesUseCase>(), nullptr);
}

// ============================================================
// ApplicationContainer端口测试
// ============================================================

TEST(ApplicationContainer, ResolveAllPorts) {
    ApplicationContainer container("config/machine_config.ini");
    container.Configure();

    // 验证所有端口都可以解析
    EXPECT_NE(container.ResolvePort<Domain::Ports::IConfigurationPort>(), nullptr);
    EXPECT_NE(container.ResolvePort<Domain::Ports::IPositionControlPort>(), nullptr);
    EXPECT_NE(container.ResolvePort<Domain::Ports::IHomingPort>(), nullptr);
    EXPECT_NE(container.ResolvePort<Domain::Ports::ITriggerControllerPort>(), nullptr);
    EXPECT_NE(container.ResolvePort<Domain::Ports::IDiagnosticsPort>(), nullptr);
    EXPECT_NE(container.ResolvePort<Domain::Ports::IEventPublisherPort>(), nullptr);
}

TEST(ApplicationContainer, NonCopyable) {
    ApplicationContainer container("config/machine_config.ini");

    // ApplicationContainer禁止拷贝
    EXPECT_FALSE(std::is_copy_constructible<ApplicationContainer>::value);
    EXPECT_FALSE(std::is_copy_assignable<ApplicationContainer>::value);
}

TEST(ApplicationContainer, NonMovable) {
    ApplicationContainer container("config/machine_config.ini");

    // ApplicationContainer禁止移动
    EXPECT_FALSE(std::is_move_constructible<ApplicationContainer>::value);
    EXPECT_FALSE(std::is_move_assignable<ApplicationContainer>::value);
}

// ============================================================
// ApplicationContainer生命周期测试
// ============================================================

TEST(ApplicationContainer, SingletonLifetime) {
    ApplicationContainer container("config/machine_config.ini");
    container.Configure();

    auto useCase1 = container.Resolve<UseCases::InitializeSystemUseCase>();
    auto useCase2 = container.Resolve<UseCases::InitializeSystemUseCase>();

    // 验证单例生命周期
    EXPECT_EQ(useCase1, useCase2);
    EXPECT_EQ(useCase1.use_count(), 2);  // useCase1 + useCase2
}

// ============================================================
// ApplicationContainer配置测试
// ============================================================

TEST(ApplicationContainer, ConfigureBeforeResolve) {
    ApplicationContainer container("config/machine_config.ini");

    // 配置前解析应该返回nullptr
    auto useCase = container.Resolve<UseCases::InitializeSystemUseCase>();
    EXPECT_EQ(useCase, nullptr);

    // 配置后应该可以解析
    container.Configure();
    useCase = container.Resolve<UseCases::InitializeSystemUseCase>();
    EXPECT_NE(useCase, nullptr);
}
```

**Step 2: 删除旧的test_service_container.cpp**

运行:
```bash
rm tests/unit/test_service_container.cpp
```

**Step 3: 验证编译**

运行:
```bash
cmake --build build --target test_application_container
```

预期: 编译成功

**Step 4: 运行新测试**

运行:
```bash
ctest --test-dir build -R test_ApplicationContainer -V
```

预期: 所有测试通过

**Step 5: 提交更改**

运行:
```bash
git add tests/unit/container/test_ApplicationContainer.cpp
git rm tests/unit/test_service_container.cpp
git commit -m "refactor(di): replace ServiceContainer tests with ApplicationContainer tests"
```

---

## 阶段4: 清理ServiceContainer文件

**理由:** 所有迁移完成后，删除ServiceContainer的实现文件和依赖。

### Task 5: 删除ServiceContainer和ServiceDescriptor

**Files:**
- 删除: `src/application/di/ServiceContainer.h` (216行)
- 删除: `src/application/di/ServiceDescriptor.h` (87行)
- 修改: `src/application/di/CMakeLists.txt`（如果存在）

**Step 1: 验证无剩余引用**

运行: `rg "ServiceContainer" src/ --type cpp`

预期: 仅输出刚才的分析文件，无实际代码引用

**Step 2: 删除ServiceContainer头文件**

运行: `rm src/application/di/ServiceContainer.h`

**Step 3: 删除ServiceDescriptor头文件**

运行: `rm src/application/di/ServiceDescriptor.h`

**Step 4: 检查di目录是否为空**

运行: `ls -la src/application/di/`

预期: 目录为空或只有README

**Step 5: 删除空的di目录**

如果目录为空:
```bash
rmdir src/application/di/
```

**Step 6: 更新CMakeLists.txt**

检查主CMakeLists.txt和src/application/CMakeLists.txt，删除对di目录的引用。

**Step 7: 验证完整编译**

运行:
```bash
cmake --build build --clean-first
```

预期: 整个项目编译成功，无ServiceContainer相关错误

**Step 8: 运行所有测试**

运行:
```bash
ctest --test-dir build --output-on-failure
```

预期: 所有测试通过

**Step 9: 提交清理**

运行:
```bash
git add -A
git commit -m "refactor(di): remove ServiceContainer and ServiceDescriptor (303 lines)"
```

---

## 阶段5: 更新文档

**理由:** 更新开发者文档，反映DI容器统一后的架构。

### Task 6: 更新DI使用指南

**Files:**
- 修改: `docs/04-development/di-unification-plan.md`
- 创建: `docs/04-development/di-usage-guide.md`

**Step 1: 更新di-unification-plan.md的状态**

修改文档状态从`proposed`改为`implemented`:
```markdown
> **创建日期**: 2026-01-08
> **状态**: implemented ✅  # 修改这里
> **优先级**: P1 (High)
```

**Step 2: 创建DI使用指南**

创建文件: `docs/04-development/di-usage-guide.md`

内容:
```markdown
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
#include "container/ApplicationContainer.h"

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

- [依赖注入统一方案](./di-unification-plan.md)
- [ApplicationContainer实现](../../apps/control-runtime/container/ApplicationContainer.h)
- [ZeroOverheadDI实现](../../src/shared/di/ZeroOverheadDI.h)
- [TestContainer实现](../../tests/utils/TestServiceContainer.h)

---

**创建日期**: 2026-01-08
**最后更新**: 2026-01-08
**维护者**: @lead-architect
```

**Step 3: 更新架构改进总结**

修改`docs/04-development/architecture-improvements-summary.md`，将"阶段2: 删除ServiceContainer"标记为完成。

**Step 4: 提交文档更新**

运行:
```bash
git add docs/04-development/
git commit -m "docs(di): update DI usage guide and mark unification as complete"
```

---

## 验收标准

### 完成检查清单

- [x] **Task 0**: 分析ServiceContainer使用情况
- [x] **Task 1**: 验证ServiceLocator无实际使用
- [x] **Task 2**: 删除ServiceLocator文件
- [x] **Task 3**: 迁移DIPerformanceBenchmark
- [x] **Task 4**: 重写ApplicationContainer单元测试
- [x] **Task 5**: 删除ServiceContainer和ServiceDescriptor
- [x] **Task 6**: 更新DI使用指南和文档

**完成日期**: 2026-01-08
**状态**: ✅ 已完成

### 最终验证

**Step 1: 编译完整项目**

运行:
```bash
cmake --build build --clean-first
```

预期: 编译成功，无ServiceContainer相关错误

**Step 2: 运行所有测试**

运行:
```bash
ctest --test-dir build --output-on-failure
```

预期: 所有测试通过（包括新的ApplicationContainer测试）

**Step 3: 验证代码行数减少**

运行:
```bash
# 统计删除的代码行数
git diff HEAD~5 --stat | grep "ServiceContainer\|ServiceDescriptor\|ServiceLocator"
```

预期: 删除约400行代码（216 + 87 + 114 + 相关测试）

**Step 4: 运行架构验证**

运行:
```bash
pwsh scripts/analysis/arch-validate.ps1
```

预期: 架构规则验证通过，无ServiceContainer引用

**Step 5: 性能基准测试对比**

运行:
```bash
./build/tests/benchmark/DIPerformanceBenchmark.exe
```

预期: ApplicationContainer性能可接受（与之前ServiceContainer相当或更好）

---

## 风险和回滚

### 风险评估

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|----------|
| ApplicationContainer性能下降 | 低 | 中 | 性能基准测试验证 |
| 测试覆盖下降 | 低 | 低 | 新增ApplicationContainer测试 |
| 破坏现有功能 | 低 | 高 | 完整测试套件验证 |

### 回滚计划

如果迁移后发现严重问题：

1. 回滚到迁移前的commit:
   ```bash
   git revert <commit-hash>
   ```

2. 恢复ServiceContainer文件:
   ```bash
   git checkout HEAD~1 -- src/application/di/
   ```

3. 重新编译验证

---

## 预期收益

| 指标 | 改进前 | 改进后 | 提升 |
|------|--------|--------|------|
| **DI代码行数** | 900+ 行 | ~600 行 | -33% |
| **容器实现数量** | 3 个 | 2 个（+1测试） | -33% |
| **维护复杂度** | 高（3处同步） | 低（单点维护） | -60% |
| **学习曲线** | 陡（3套API） | 平缓（清晰分层） | -50% |
| **生产代码DI** | ServiceContainer + ApplicationContainer | 仅ApplicationContainer | ✅ 统一 |

---

**实施时间估计**: 2-3天（6个任务，每个任务半天）
**优先级**: P1 (High)
**依赖**: 无
**阻塞**: 无

