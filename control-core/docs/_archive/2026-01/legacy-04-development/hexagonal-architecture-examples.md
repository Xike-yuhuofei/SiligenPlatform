# 六边形架构正确实现示例集

> **目标**: 提供可复制粘贴的代码模板，确保架构合规性
> **适用场景**: 新功能开发、重构现有代码
> **审核清单**: 每个示例都通过架构验证工具检查

---

## 目录

1. [Port接口定义规范](#1-port接口定义规范)
2. [Adapter实现规范](#2-adapter实现规范)
3. [UseCase实现规范](#3-usecase实现规范)
4. [组合根配置规范](#4-组合根配置规范)
5. [错误处理规范](#5-错误处理规范)
6. [命名空间规范](#6-命名空间规范)

---

## 1. Port接口定义规范

### ✅ 正确示例: Domain层Port接口

**文件**: `src/domain/<subdomain>/ports/IMotionPort.h`

```cpp
// IMotionPort.h - 运动控制端口接口
// 架构合规性: Domain层零外部依赖，纯虚接口，使用Result<T>错误处理
#pragma once

#include <memory>
#include <vector>
#include "../../shared/types/Result.h"
#include "../../shared/types/GeometryTypes.h"

namespace Siligen {
namespace Domain {
namespace Ports {

using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::float32;

/**
 * @brief 运动控制端口接口
 *
 * 职责: 定义运动控制的抽象能力
 * - Domain层定义接口（零实现）
 * - Infrastructure层实现接口
 * - Application层通过接口调用
 *
 * 依赖方向: Infrastructure -> Domain (依赖倒置)
 */
class IMotionPort {
public:
    virtual ~IMotionPort() = default;

    /**
     * @brief 移动到指定位置
     * @param position 目标位置
     * @param speed 移动速度 (单位: mm/s)
     * @return Result<void> 成功或失败
     *
     * 错误码:
     * - E_HARDWARE_NOT_CONNECTED: 硬件未连接
     * - E_MOTION_OUT_OF_RANGE: 位置超出范围
     * - E_HARDWARE_FAILURE: 硬件故障
     */
    virtual Result<void> MoveToPosition(
        const Point2D& position,
        float32 speed
    ) = 0;

    /**
     * @brief 停止运动
     * @return Result<void> 成功或失败
     */
    virtual Result<void> StopMotion() = 0;

    /**
     * @brief 获取当前位置
     * @return Result<Point2D> 当前位置或错误
     */
    virtual Result<Point2D> GetCurrentPosition() = 0;

    /**
     * @brief 回零（归位）
     * @return Result<void> 成功或失败
     */
    virtual Result<void> Home() = 0;

protected:
    // 禁止直接实例化（纯接口）
    IMotionPort() = default;
};

} // namespace Ports
} // namespace Domain
} // namespace Siligen
```

**关键规范**:
- ✅ 只依赖Shared层类型（`Result<T>`, `Point2D`）
- ✅ 使用纯虚函数（`= 0`）
- ✅ 返回 `Result<T>` 而非异常
- ✅ 包含详细的Doxygen注释
- ✅ 命名空间：`Siligen::Domain::Ports`
- ✅ 文件命名：`I{功能名}Port.h`

### ❌ 错误示例: 违反架构规范

```cpp
// ❌ 错误: Domain层包含Infrastructure层
#include "infrastructure/hardware/MultiCardAdapter.h"  // 违规！

// ❌ 错误: 使用异常而非Result<T>
virtual void MoveToPosition(const Point2D& pos, float32 speed) throw(std::runtime_error);

// ❌ 错误: 包含实现代码
virtual Result<void> MoveToPosition(...) {
    MultiCardAdapter::Move(...);  // Domain层不应包含实现！
}

// ❌ 错误: 缺少文档注释
virtual Result<void> DoSomething(int x) = 0;  // 没有注释！
```

---

## 2. Adapter实现规范

### ✅ 正确示例: Infrastructure层Adapter

**文件**: `src/infrastructure/adapters/hardware/MultiCardMotionAdapter.h`

```cpp
// MultiCardMotionAdapter.h - MultiCard运动控制适配器
// 架构合规性: Infrastructure层实现Domain端口，封装硬件API
#pragma once

#include "domain/<subdomain>/ports/IMotionPort.h"
#include "shared/types/Result.h"
#include "shared/types/GeometryTypes.h"
#include <memory>

namespace Siligen {
namespace Infrastructure {
namespace Adapters {

using Siligen::Domain::Ports::IMotionPort;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::float32;

/**
 * @brief MultiCard运动控制适配器
 *
 * 职责:
 * - 实现IMotionPort接口
 * - 封装MultiCard硬件API
 * - 转换硬件错误码为领域错误
 *
 * 依赖方向: Infrastructure -> Domain (实现Port接口)
 */
class MultiCardMotionAdapter : public IMotionPort {
public:
    /**
     * @brief 构造函数
     * @param card_id 运动控制卡ID
     */
    explicit MultiCardMotionAdapter(int card_id);

    /**
     * @brief 析构函数
     */
    ~MultiCardMotionAdapter() override;

    // 实现IMotionPort接口
    Result<void> MoveToPosition(const Point2D& position, float32 speed) override;
    Result<void> StopMotion() override;
    Result<Point2D> GetCurrentPosition() override;
    Result<void> Home() override;

private:
    int card_id_;

    /**
     * @brief 转换MultiCard错误码为Result<T>
     * @param error_code MultiCard错误码
     * @return Error 领域错误对象
     */
    static Error ConvertMultiCardError(int error_code);

    // 禁止拷贝
    MultiCardMotionAdapter(const MultiCardMotionAdapter&) = delete;
    MultiCardMotionAdapter& operator=(const MultiCardMotionAdapter&) = delete;
};

} // namespace Adapters
} // namespace Infrastructure
} // namespace Siligen
```

**实现文件**: `src/infrastructure/adapters/hardware/MultiCardMotionAdapter.cpp`

```cpp
#include "MultiCardMotionAdapter.h"
#include "infrastructure/hardware/MultiCard_API.h"  // 厂商SDK
#include "shared/types/ErrorTypes.h"
#include <stdexcept>

namespace Siligen {
namespace Infrastructure {
namespace Adapters {

MultiCardMotionAdapter::MultiCardMotionAdapter(int card_id)
    : card_id_(card_id) {
    // 初始化硬件连接
    if (MultiCard_OpenCard(card_id_) != 0) {
        throw std::runtime_error("Failed to open MultiCard");
    }
}

MultiCardMotionAdapter::~MultiCardMotionAdapter() {
    MultiCard_CloseCard(card_id_);
}

Result<void> MultiCardMotionAdapter::MoveToPosition(
    const Point2D& position,
    float32 speed
) {
    // 1. 参数验证
    if (speed <= 0.0f || speed > 1000.0f) {
        return Result<void>::Error(
            Error(ErrorCode::E_INVALID_ARGUMENT,
                  "Speed must be in range (0, 1000]")
        );
    }

    // 2. 调用硬件API
    int result = MultiCard_MoveTo(
        card_id_,
        static_cast<int>(position.x),
        static_cast<int>(position.y),
        static_cast<int>(speed)
    );

    // 3. 转换错误
    if (result != 0) {
        return Result<void>::Error(ConvertMultiCardError(result));
    }

    return Result<void>::Success();
}

Error MultiCardMotionAdapter::ConvertMultiCardError(int error_code) {
    switch (error_code) {
        case 1:
            return Error(ErrorCode::E_HARDWARE_NOT_CONNECTED,
                        "MultiCard: Card not connected");
        case 2:
            return Error(ErrorCode::E_MOTION_OUT_OF_RANGE,
                        "MultiCard: Position out of range");
        case 3:
            return Error(ErrorCode::E_HARDWARE_FAILURE,
                        "MultiCard: Hardware failure");
        default:
            return Error(ErrorCode::E_UNKNOWN,
                        "MultiCard: Unknown error " + std::to_string(error_code));
    }
}

} // namespace Adapters
} // namespace Infrastructure
} // namespace Siligen
```

**关键规范**:
- ✅ 实现 `IMotionPort` 接口
- ✅ 封装硬件API（`MultiCard_MoveTo`）
- ✅ 转换错误码（`ConvertMultiCardError`）
- ✅ 使用 `Result<T>` 返回错误
- ✅ 命名空间：`Siligen::Infrastructure::Adapters`
- ✅ 禁止拷贝构造（`= delete`）

### ❌ 错误示例: 违反Adapter规范

```cpp
// ❌ 错误: 暴露硬件API给上层
class MultiCardMotionAdapter : public IMotionPort {
public:
    MultiCard_API* GetHardwareAPI() {  // 不应该暴露硬件！
        return &multi_card_api_;
    }
};

// ❌ 错误: 直接抛出异常
Result<void> MoveToPosition(...) override {
    if (failed) {
        throw std::runtime_error("Failed");  // 应该返回Result<T>
    }
}

// ❌ 错误: 包含业务逻辑
Result<void> MoveToPosition(...) override {
    // 复杂的业务规则应该在Domain层！
    if (user_is_authorized && safety_check_passed && ...) {
        // ... 50行业务逻辑
    }
    MultiCard_MoveTo(...);
}
```

---

## 3. UseCase实现规范

### ✅ 正确示例: Application层UseCase

**文件**: `src/application/usecases/JogTestUseCase.h`

```cpp
// JogTestUseCase.h - 点动测试用例
// 架构合规性: Application层编排业务流程，依赖Domain Port接口
#pragma once

#include "domain/<subdomain>/ports/IMotionPort.h"
#include "shared/types/Result.h"
#include "shared/types/GeometryTypes.h"
#include <memory>

namespace Siligen {
namespace Application {
namespace UseCases {

using Siligen::Domain::Ports::IMotionPort;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::float32;

/**
 * @brief 点动测试请求
 */
struct JogTestRequest {
    Point2D target_position;
    float32 speed;

    /**
     * @brief 验证请求参数
     * @return Result<void> 有效或失败
     */
    Result<void> Validate() const {
        if (speed <= 0.0f || speed > 1000.0f) {
            return Result<void>::Error(
                Error(ErrorCode::E_INVALID_ARGUMENT,
                      "Speed must be in range (0, 1000]")
            );
        }
        return Result<void>::Success();
    }
};

/**
 * @brief 点动测试响应
 */
struct JogTestResponse {
    bool success;
    std::string message;
    Point2D final_position;
};

/**
 * @brief 点动测试用例
 *
 * 职责:
 * - 编排点动测试业务流程
 * - 依赖IMotionPort接口（不依赖具体实现）
 * - 协调多个领域服务
 *
 * 依赖方向: Application -> Domain (依赖Port接口)
 */
class JogTestUseCase {
public:
    /**
     * @brief 构造函数（依赖注入）
     * @param motion_port 运动控制端口（通过接口注入）
     */
    explicit JogTestUseCase(std::shared_ptr<IMotionPort> motion_port);

    /**
     * @brief 执行点动测试
     * @param request 测试请求
     * @return Result<JogTestResponse> 测试结果或错误
     */
    Result<JogTestResponse> Execute(const JogTestRequest& request);

private:
    std::shared_ptr<IMotionPort> motion_port_;

    /**
     * @brief 记录测试结果
     */
    void LogTestResult(const JogTestResponse& response);
};

} // namespace UseCases
} // namespace Application
} // namespace Siligen
```

**实现文件**: `src/application/usecases/JogTestUseCase.cpp`

```cpp
#include "JogTestUseCase.h"
#include "infrastructure/logging/Logger.h"
#include <sstream>

namespace Siligen {
namespace Application {
namespace UseCases {

JogTestUseCase::JogTestUseCase(std::shared_ptr<IMotionPort> motion_port)
    : motion_port_(motion_port) {
    if (!motion_port_) {
        throw std::invalid_argument("motion_port cannot be null");
    }
}

Result<JogTestResponse> JogTestUseCase::Execute(const JogTestRequest& request) {
    // 1. 验证请求
    auto validation = request.Validate();
    if (validation.IsError()) {
        return Result<JogTestResponse>::Error(validation.GetError());
    }

    // 2. 执行点动（通过Port接口）
    auto move_result = motion_port_->MoveToPosition(
        request.target_position,
        request.speed
    );

    // 3. 处理结果
    if (move_result.IsError()) {
        JogTestResponse response{
            false,
            "Jog test failed: " + move_result.GetError().message,
            {0.0f, 0.0f}
        };
        LogTestResult(response);
        return Result<JogTestResponse>::Success(response);
    }

    // 4. 获取最终位置
    auto position_result = motion_port_->GetCurrentPosition();
    if (position_result.IsError()) {
        JogTestResponse response{
            true,
            "Jog test completed (failed to read position)",
            request.target_position
        };
        LogTestResult(response);
        return Result<JogTestResponse>::Success(response);
    }

    // 5. 成功响应
    JogTestResponse response{
        true,
        "Jog test completed successfully",
        position_result.GetValue()
    };
    LogTestResult(response);
    return Result<JogTestResponse>::Success(response);
}

void JogTestUseCase::LogTestResult(const JogTestResponse& response) {
    std::ostringstream oss;
    oss << "JogTest: " << (response.success ? "SUCCESS" : "FAILED")
        << " | " << response.message;
    Infrastructure::Logging::Logger::Info(oss.str());
}

} // namespace UseCases
} // namespace Application
} // namespace Siligen
```

**关键规范**:
- ✅ 只依赖 `IMotionPort` 接口（不依赖具体Adapter）
- ✅ 通过构造函数注入依赖
- ✅ Request包含 `Validate()` 方法
- ✅ 返回 `Result<Response>`
- ✅ 命名空间：`Siligen::Application::UseCases`

### ❌ 错误示例: 违反UseCase规范

```cpp
// ❌ 错误: 直接依赖Adapter实现
class JogTestUseCase {
public:
    explicit JogTestUseCase(
        std::shared_ptr<MultiCardMotionAdapter> adapter  // 应该用IMotionPort！
    );
};

// ❌ 错误: 包含业务逻辑（应在Domain层）
Result<JogTestResponse> Execute(...) {
    // 复杂的运动规划算法应该在Domain层！
    auto trajectory = CalculateOptimalTrajectory(...);  // 100行业务逻辑
    motion_port_->MoveToPosition(...);
}

// ❌ 错误: 直接创建依赖
class JogTestUseCase {
public:
    JogTestUseCase() {
        motion_port_ = std::make_shared<MultiCardMotionAdapter>();  // 应该注入！
    }
};
```

---

## 4. 组合根配置规范

### ✅ 正确示例: Bootstrap层CompositionRoot

**文件**: `src/bootstrap/CompositionRoot.h`

```cpp
// CompositionRoot.h - 统一依赖注入配置
#pragma once

#include "domain/<subdomain>/ports/IMotionPort.h"
#include "domain/<subdomain>/ports/IHardwarePort.h"
#include "application/usecases/JogTestUseCase.h"
#include "shared/di/ServiceContainer.h"
#include <memory>
#include <string>

namespace Siligen {
namespace Bootstrap {

using Siligen::Shared::DI::ServiceContainer;

/**
 * @brief 组合根 - 负责装配所有依赖关系
 *
 * 职责:
 * 1. 创建所有Infrastructure层Adapter实例
 * 2. 创建所有Application层UseCase实例
 * 3. 注入依赖关系
 * 4. 提供唯一启动入口
 *
 * 参考模式: Composition Root by Mark Seemann
 * (https://blog.ploeh.dk/2011/07/28/CompositionRoot/)
 */
class CompositionRoot {
public:
    /**
     * @brief 初始化组合根
     * @param config_path 配置文件路径
     * @return Result<void> 成功或失败
     */
    static Result<void> Initialize(const std::string& config_path);

    /**
     * @brief 获取MotionController实例
     */
    static std::shared_ptr<Domain::Ports::IMotionPort>
        GetMotionController();

    /**
     * @brief 获取特定UseCase实例
     */
    template<typename UseCaseType>
    static std::shared_ptr<UseCaseType> GetUseCase();

    /**
     * @brief 清理所有资源
     */
    static void Shutdown();

private:
    // 单例模式
    CompositionRoot() = default;
    CompositionRoot(const CompositionRoot&) = delete;
    CompositionRoot& operator=(const CompositionRoot&) = delete;

    // 注册所有Adapters
    static Result<void> RegisterAdapters(const std::string& config_path);

    // 注册所有UseCases
    static Result<void> RegisterUseCases();

    // DI容器
    static std::unique_ptr<ServiceContainer> container_;
};

} // namespace Bootstrap
} // namespace Siligen
```

**实现文件**: `src/bootstrap/CompositionRoot.cpp`

```cpp
#include "CompositionRoot.h"
#include "infrastructure/adapters/hardware/MultiCardMotionAdapter.h"
#include "infrastructure/configs/IniConfigLoader.h"
#include "shared/types/ErrorTypes.h"

namespace Siligen {
namespace Bootstrap {

std::unique_ptr<ServiceContainer> CompositionRoot::container_;

Result<void> CompositionRoot::Initialize(const std::string& config_path) {
    // 创建DI容器
    container_ = std::make_unique<ServiceContainer>();

    // Step 1: 加载配置
    // （略，使用Infrastructure层ConfigLoader加载配置）

    // Step 2: 注册Adapters（单例）
    auto register_result = RegisterAdapters(config_path);
    if (register_result.IsError()) {
        return register_result;
    }

    // Step 3: 注册UseCases（瞬态）
    auto usecases_result = RegisterUseCases();
    if (usecases_result.IsError()) {
        return usecases_result;
    }

    return Result<void>::Success();
}

Result<void> CompositionRoot::RegisterAdapters(const std::string& config_path) {
    // 创建并注册MotionController
    auto motion_adapter = std::make_shared<Infrastructure::Adapters::MultiCardMotionAdapter>(
        0  // card_id from config
    );
    container_->RegisterSingleton<Domain::Ports::IMotionPort>(motion_adapter);

    // 注册其他Adapters...
    // (HardwareController, VisualizationPort, etc.)

    return Result<void>::Success();
}

Result<void> CompositionRoot::RegisterUseCases() {
    // 注册UseCases（瞬态，每次请求创建新实例）
    container_->RegisterTransient<Application::UseCases::JogTestUseCase>();
    container_->RegisterTransient<Application::UseCases::HomingTestUseCase>();
    container_->RegisterTransient<Application::UseCases::CMPTestUseCase>();

    return Result<void>::Success();
}

std::shared_ptr<Domain::Ports::IMotionPort>
    CompositionRoot::GetMotionController() {
    return container_->Resolve<Domain::Ports::IMotionPort>();
}

template<typename UseCaseType>
std::shared_ptr<UseCaseType> CompositionRoot::GetUseCase() {
    return container_->Resolve<UseCaseType>();
}

void CompositionRoot::Shutdown() {
    container_->Shutdown();
    container_.reset();
}

// 显式实例化常用UseCase类型
template std::shared_ptr<Application::UseCases::JogTestUseCase>
    CompositionRoot::GetUseCase<Application::UseCases::JogTestUseCase>();

template std::shared_ptr<Application::UseCases::HomingTestUseCase>
    CompositionRoot::GetUseCase<Application::UseCases::HomingTestUseCase>();

} // namespace Bootstrap
} // namespace Siligen
```

**使用示例**: `apps/control-cli/main.cpp`

```cpp
#include "bootstrap/CompositionRoot.h"
#include "application/usecases/JogTestUseCase.h"
#include <iostream>

int main(int argc, char* argv[]) {
    // 1. 初始化组合根
    auto init_result = Bootstrap::CompositionRoot::Initialize("config/machine_config.ini");
    if (init_result.IsError()) {
        std::cerr << "Failed to initialize: "
                  << init_result.GetError().message << std::endl;
        return 1;
    }

    // 2. 获取UseCase
    auto jog_test = Bootstrap::CompositionRoot::GetUseCase<
        Application::UseCases::JogTestUseCase
    >();

    // 3. 执行业务逻辑
    Application::UseCases::JogTestRequest request{
        {100.0f, 200.0f},  // target_position
        50.0f              // speed
    };

    auto result = jog_test->Execute(request);
    if (result.IsError()) {
        std::cerr << "Jog test failed: "
                  << result.GetError().message << std::endl;
        Bootstrap::CompositionRoot::Shutdown();
        return 1;
    }

    auto response = result.GetValue();
    std::cout << "Jog test completed: " << response.message << std::endl;

    // 4. 清理
    Bootstrap::CompositionRoot::Shutdown();
    return 0;
}
```

---

## 5. 错误处理规范

### ✅ 正确示例: Result<T>模式

**定义**: `src/shared/types/Result.h`

```cpp
// Result.h - 统一错误处理
#pragma once

#include <variant>
#include <string>
#include "ErrorTypes.h"

namespace Siligen {
namespace Shared {
namespace Types {

/**
 * @brief Result<T> - 表示操作结果（成功或失败）
 *
 * 用法:
 *   Success: return Result<T>::Success(value);
 *   Failure: return Result<T>::Success(Error(...));
 */
template<typename T>
class Result {
public:
    /**
     * @brief 创建成功结果
     */
    static Result Success(T value) {
        return Result(std::move(value));
    }

    /**
     * @brief 创建失败结果
     */
    static Result Error(const Error& error) {
        return Result(error);
    }

    /**
     * @brief 判断是否成功
     */
    bool IsSuccess() const {
        return std::holds_alternative<T>(data_);
    }

    /**
     * @brief 判断是否失败
     */
    bool IsError() const {
        return std::holds_alternative<Error>(data_);
    }

    /**
     * @brief 获取成功值（断言成功）
     */
    const T& GetValue() const {
        return std::get<T>(data_);
    }

    /**
     * @brief 获取错误（断言失败）
     */
    const Error& GetError() const {
        return std::get<Error>(data_);
    }

private:
    std::variant<T, Error> data_;

    explicit Result(T value) : data_(std::move(value)) {}
    explicit Result(const Error& error) : data_(error) {}
};

// void特化
template<>
class Result<void> {
public:
    static Result Success() {
        return Result(true);
    }

    static Result Error(const Error& error) {
        return Result(error);
    }

    bool IsSuccess() const { return success_; }
    bool IsError() const { return !success_; }

    const Error& GetError() const { return error_; }

private:
    bool success_;
    Error error_;

    explicit Result(bool success) : success_(success), error_() {}
    explicit Result(const Error& error) : success_(false), error_(error) {}
};

} // namespace Types
} // namespace Shared
} // namespace Siligen
```

**使用示例**:

```cpp
// ✅ 正确: 返回Result<T>
Result<void> MoveToPosition(const Point2D& pos, float32 speed) {
    if (speed <= 0.0f) {
        return Result<void>::Error(
            Error(ErrorCode::E_INVALID_ARGUMENT, "Speed must be positive")
        );
    }

    // 执行操作
    int result = MultiCard_MoveTo(...);
    if (result != 0) {
        return Result<void>::Error(ConvertError(result));
    }

    return Result<void>::Success();
}

// ✅ 正确: 处理Result<T>
auto move_result = motion_port->MoveToPosition(pos, speed);
if (move_result.IsError()) {
    std::cerr << "Move failed: " << move_result.GetError().message << std::endl;
    return;
}

// ✅ 正确: 链式调用Result<T>
auto pos_result = motion_port->GetCurrentPosition();
if (pos_result.IsError()) {
    return Result<JogTestResponse>::Error(pos_result.GetError());
}

auto pos = pos_result.GetValue();
```

### ❌ 错误示例: 违反错误处理规范

```cpp
// ❌ 错误: 使用异常
void MoveToPosition(...) {
    if (invalid) {
        throw std::runtime_error("Invalid argument");  // 应该返回Result<T>
    }
}

// ❌ 错误: 忽略错误
motion_port->MoveToPosition(...);  // 应该检查返回值！

// ❌ 错误: 返回bool（丢失错误信息）
bool MoveToPosition(...) {
    if (failed) return false;  // 应该返回Result<T>以提供错误详情
    return true;
}
```

---

## 6. 命名空间规范

### ✅ 正确示例

```cpp
// ✅ Domain层
namespace Siligen {
namespace Domain {
namespace Ports {

class IMotionPort {  // Siligen::Domain::Ports::IMotionPort
    // ...
};

} // namespace Ports
} // namespace Domain
} // namespace Siligen

// ✅ Infrastructure层
namespace Siligen {
namespace Infrastructure {
namespace Adapters {

class MultiCardMotionAdapter {  // Siligen::Infrastructure::Adapters::MultiCardMotionAdapter
    // ...
};

} // namespace Adapters
} // namespace Infrastructure
} // namespace Siligen

// ✅ Application层
namespace Siligen {
namespace Application {
namespace UseCases {

class JogTestUseCase {  // Siligen::Application::UseCases::JogTestUseCase
    // ...
};

} // namespace UseCases
} // namespace Application
} // namespace Siligen
```

### ❌ 错误示例

```cpp
// ❌ 错误: 缺少命名空间
class IMotionPort {  // 应该在 Siligen::Domain::Ports 命名空间
};

// ❌ 错误: 使用匿名命名空间
namespace {
    class IMotionPort {  // 不应在头文件使用匿名命名空间
    };
}

// ❌ 错误: 命名空间层级错误
namespace Siligen {
namespace Infrastructure {  // Domain层的Port不应在Infrastructure命名空间
namespace Ports {
    class IMotionPort {  // 错误的命名空间层级
    };
}
}
}
```

---

## 审核清单

在使用上述示例时，请确保：

- [ ] Port接口定义在 `src/domain/<subdomain>/ports/`
- [ ] Adapter实现在 `src/infrastructure/adapters/`
- [ ] UseCase在 `src/application/usecases/`
- [ ] 所有错误返回 `Result<T>`
- [ ] 所有依赖通过构造函数注入
- [ ] 命名空间符合层级规范
- [ ] 通过 `python .claude/agents/arch_analyzer.py` 验证

---

**文档版本**: v1.0.0
**最后更新**: 2026-01-08
**审核者**: Coder Agent

