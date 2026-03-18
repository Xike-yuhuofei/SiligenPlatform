# 测试基础设施指南

**版本**: 1.0.0
**创建日期**: 2025-12-03
**任务**: Phase 5.2 (T061-T063) - 单元测试基础设施

##  概述

本指南介绍 Siligen 工业点胶机控制系统的单元测试基础设施，包括 Google Test 框架配置、测试工具库和服务容器隔离机制。

##  已完成的任务

### T061: 配置单元测试框架

**实现内容**:
- 在 `CMakeLists.txt` 中集成 Google Test
- 使用 `FetchContent` 自动下载 Google Test v1.14.0
- 配置 CTest 测试运行器
- 创建 `tests/CMakeLists.txt` 测试配置
- 提供 `scripts/build_tests.ps1` 构建脚本

**关键文件**:
- `CMakeLists.txt` (第487-523行)
- `tests/CMakeLists.txt`
- `scripts/build_tests.ps1`
- `tests/unit/test_gtest_setup.cpp` (验证配置)

**构建命令**:
```bash
# 启用测试构建
cmake -B build -S . -DSILIGEN_BUILD_TESTS=ON

# 构建测试
cmake --build build --config Release

# 运行测试
cd build && ctest --output-on-failure

# 或使用PowerShell脚本
.\scripts\build_tests.ps1
```

### T062: 创建测试用例基类和通用测试工具

**实现内容**:
- `TestFixture.h` - 5种测试夹具基类
  - `BaseTestFixture` - 基础测试夹具
  - `MockServiceFixture` - 带Mock服务的夹具
  - `ParameterizedTestFixture` - 参数化测试夹具
  - `PerformanceTestFixture` - 性能测试夹具
  - `TempDirTestFixture` - 临时目录测试夹具

- `TestHelper.h` - 丰富的测试辅助工具
  - Result<T>断言宏 (EXPECT_RESULT_SUCCESS, EXPECT_RESULT_VALUE_EQ等)
  - 浮点数比较工具 (FloatNearlyEqual, PointNearlyEqual)
  - 容器断言宏 (EXPECT_CONTAINER_SIZE, EXPECT_CONTAINER_CONTAINS等)
  - 字符串断言宏 (EXPECT_STRING_CONTAINS, EXPECT_STRING_STARTS_WITH等)
  - 测试数据生成器 (GenerateTestPoints, GenerateFloatSequence)
  - 执行时间测量 (MeasureExecutionTimeMs, MeasureExecutionTimeUs)

**关键文件**:
- `tests/utils/TestFixture.h`
- `tests/utils/TestHelper.h`
- `tests/unit/test_helper_demo.cpp` (使用示例)

**使用示例**:
```cpp
#include "../utils/TestFixture.h"
#include "../utils/TestHelper.h"

class MyTest : public BaseTestFixture {
protected:
    void OnSetUp() override {
        test_value_ = 42;
    }
    int test_value_;
};

TEST_F(MyTest, UseHelper) {
    auto result = DoSomething(test_value_);
    EXPECT_RESULT_SUCCESS(result);
    EXPECT_RESULT_VALUE_EQ(result, 84);
}
```

### T063: 实现测试服务容器隔离机制

**实现内容**:
- `TestServiceContainer` - 轻量级DI容器
  - 类型安全的服务注册和解析
  - Singleton和Transient生命周期支持
  - 自动清理机制
  - 线程安全设计

- `TestServiceContainerBuilder` - 流式API构建器
- `ServiceContainerTestFixture` - 带容器的测试夹具

**关键文件**:
- `tests/utils/TestServiceContainer.h`
- `tests/unit/test_service_container.cpp` (功能验证)

**使用示例**:
```cpp
class MyServiceTest : public ServiceContainerTestFixture {
protected:
    void OnSetUpContainer() override {
        // 注册Mock服务
        auto mock_hardware = std::make_shared<MockHardwareController>();
        RegisterSingleton<IHardwareController>(mock_hardware);
    }
};

TEST_F(MyServiceTest, TestWithContainer) {
    auto hardware = Resolve<IHardwareController>();
    // 使用服务进行测试
}
```

##  文件结构

```
tests/
├── CMakeLists.txt                      # 测试配置
├── mocks/                              # Mock测试替身 (T058-T060)
│   ├── MockHardwareController.h
│   ├── MockLoggingService.h
│   ├── MockConfigManager.h
│   └── README.md
├── utils/                              # 测试工具库 (T062-T063)
│   ├── TestFixture.h                   # 测试夹具基类
│   ├── TestHelper.h                    # 测试辅助工具
│   ├── TestServiceContainer.h          # 服务容器
│   └── README.md
└── unit/                               # 单元测试
    ├── test_gtest_setup.cpp            # Google Test配置验证
    ├── test_helper_demo.cpp            # 测试工具使用示例
    └── test_service_container.cpp      # 服务容器功能验证
```

##  快速开始

### 1. 构建测试

```powershell
# 方法1: 使用脚本 (推荐)
.\scripts\build_tests.ps1

# 方法2: 手动构建
cmake -B build -S . -DSILIGEN_BUILD_TESTS=ON -DSILIGEN_FETCH_GTEST=ON
cmake --build build --config Release
```

### 2. 运行测试

```bash
# 运行所有测试
cd build
ctest --output-on-failure --verbose

# 运行特定测试
.\bin\Release\unit_tests.exe --gtest_filter=GoogleTestSetup.*

# 显示测试列表
.\bin\Release\unit_tests.exe --gtest_list_tests
```

### 3. 编写第一个测试

创建 `tests/unit/test_my_feature.cpp`:

```cpp
#include <gtests/gtest.h>
#include "../utils/TestFixture.h"
#include "../utils/TestHelper.h"
#include "../mocks/MockHardwareController.h"

using namespace Siligen::Test::Utils;
using namespace Siligen::Test::Mocks;

class MyFeatureTest : public ServiceContainerTestFixture {
protected:
    void OnSetUpContainer() override {
        mock_hardware_ = std::make_shared<MockHardwareController>();
        RegisterSingleton<IHardwareController>(mock_hardware_);
    }

    std::shared_ptr<MockHardwareController> mock_hardware_;
};

TEST_F(MyFeatureTest, BasicTest) {
    // Arrange
    mock_hardware_->SetConnectResult(Result<void>::Success());

    // Act
    auto hardware = Resolve<IHardwareController>();
    auto result = hardware->Connect(ConnectionConfig{});

    // Assert
    EXPECT_RESULT_SUCCESS(result);
    EXPECT_TRUE(mock_hardware_->WasConnectCalled());
}
```

##  测试覆盖的功能

### 已实现

-  Google Test框架集成
-  基础测试夹具类
-  Mock服务集成
-  参数化测试支持
-  性能测试支持
-  服务容器隔离
-  丰富的断言宏
-  测试数据生成器

### 待实现 (后续任务)

-  T064-T066: 业务逻辑单元测试
-  T067-T068: 配置和日志单元测试
-  T069-T072: 集成测试框架

##  最佳实践

### 1. 测试命名规范

```cpp
// 格式: Component_Method_Scenario_ExpectedBehavior
TEST(MotionService, MoveToPosition_WithValidTarget_ReturnsSuccess)
TEST(MotionService, MoveToPosition_WithInvalidTarget_ReturnsError)
```

### 2. AAA模式

```cpp
TEST_F(MyTest, Example) {
    // Arrange: 准备测试数据
    mock_hardware_->SetConnectResult(Result<void>::Success());

    // Act: 执行被测代码
    auto result = service.Connect(config);

    // Assert: 验证结果
    EXPECT_RESULT_SUCCESS(result);
}
```

### 3. 测试隔离

- 每个测试用例使用独立的容器
- 在 `SetUp()` 中初始化
- 在 `TearDown()` 中清理
- 避免测试间的状态共享

### 4. Mock配置最小化

```cpp
// 好: 只配置需要的
mock_hardware_->SetConnectResult(Result<void>::Success());

// 不好: 配置过多
mock_hardware_->SetConnectResult(...);
mock_hardware_->SetDisconnectResult(...);
mock_hardware_->SetEnableAxisResult(...);  // 测试中不需要
```

##  测试示例

### 示例1: 简单Result<T>测试

```cpp
TEST(MyTest, ResultTest) {
    auto result = CalculateSum(2, 3);
    EXPECT_RESULT_SUCCESS(result);
    EXPECT_RESULT_VALUE_EQ(result, 5);
}
```

### 示例2: 使用Mock服务

```cpp
TEST_F(MyServiceTest, WithMock) {
    // 配置Mock
    mock_hardware_->SetMoveToPositionResult(Result<void>::Success());

    // 使用服务
    auto hardware = Resolve<IHardwareController>();
    auto result = hardware->MoveToPosition(Point2D{10, 20}, 5.0f);

    // 验证
    EXPECT_RESULT_SUCCESS(result);
    EXPECT_TRUE(mock_hardware_->WasMoveToPositionCalled());
    EXPECT_POINT_NEAR(
        mock_hardware_->GetLastMovePosition(),
        Point2D{10, 20},
        0.001f
    );
}
```

### 示例3: 参数化测试

```cpp
class ParameterizedTest : public ParameterizedTestFixture<std::pair<int, int>> {
};

TEST_P(ParameterizedTest, TestSum) {
    auto [a, b] = GetTestParam();
    EXPECT_GT(a + b, 0);
}

INSTANTIATE_TEST_SUITE_P(
    PositiveSums,
    ParameterizedTest,
    ::testing::Values(
        std::make_pair(1, 2),
        std::make_pair(5, 10),
        std::make_pair(100, 200)
    )
);
```

##  相关文档

- [Mock测试替身文档](../../tests/mocks/README.md) - T058-T060实现
- [测试工具库文档](../../tests/utils/README.md) - 详细使用指南
- [Google Test官方文档](https://google.github.io/googletests/)

##  持续改进

### 下一步计划

1. **T064-T068**: 编写具体业务逻辑的单元测试
2. **T069-T072**: 建立集成测试框架
3. **覆盖率目标**: 达到90%以上的代码覆盖率
4. **CI/CD集成**: 配置持续集成测试流水线

### 反馈和改进

如果您在使用测试基础设施时遇到问题或有改进建议，请：
1. 查看 `tests/utils/README.md` 中的详细文档
2. 参考 `tests/unit/test_helper_demo.cpp` 中的示例
3. 检查 `tests/unit/test_service_container.cpp` 中的用法

##  任务进度

**Phase 5 User Story 3: 提升系统可测试性**

| 任务 | 状态 | 说明 |
|------|------|------|
| T058 |  | MockHardwareController |
| T059 |  | MockLoggingService |
| T060 |  | MockConfigManager |
| T061 |  | Google Test配置 |
| T062 |  | 测试用例基类和工具 |
| T063 |  | 服务容器隔离 |
| T064-T072 |  | 待实现 |

**进度**: 6/15 (40%)

---

**文档版本**: 1.0.0
**最后更新**: 2025-12-03
**维护者**: Claude Code