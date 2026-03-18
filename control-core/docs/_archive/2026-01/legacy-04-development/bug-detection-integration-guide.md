# Bug 检测系统集成指南

**版本:** 1.0.0
**最后更新:** 2026-01-09
**适用人群:** 框架开发者、集成工程师

---

## 概述

本文档详细说明如何将 Bug 检测系统集成到现有应用中。

### 架构原则

本系统严格遵循六边形架构原则：

```
Application Layer (ErrorHandlingUseCase, BugRecorderService)
  ↓ (依赖)
Domain Layer (Ports: Bug Store Port 已移除, IStacktraceProviderPort, etc.)
  ↑ (实现)
Infrastructure Layer (Adapters: FileBugStoreAdapter, BoostStacktraceAdapter, etc.)
```

### 关键约束

- ✅ Application 层负责错误捕获
- ✅ Domain 层保持纯净（无 IO/线程操作）
- ✅ 使用依赖注入，无单例模式
- ✅ 所有适配器通过 DI 容器注册

---

## 集成步骤

### 步骤 1: 添加端口到 ApplicationContainer

在 `src/application/container/ApplicationContainer.h` 中添加端口成员变量：

```cpp
class ApplicationContainer {
private:
    // Bug 检测系统端口
    std::shared_ptr<Domain::Ports::IStacktraceProviderPort> stacktrace_port_;
    std::shared_ptr<Domain::Ports::IClockPort> clock_port_;
    std::shared_ptr<Domain::Ports::IEnvironmentInfoProviderPort> env_info_port_;

    // Bug 检测系统服务
    std::shared_ptr<UseCases::BugRecorderService> bug_recorder_service_;
    std::shared_ptr<UseCases::ErrorHandlingUseCase> error_handling_use_case_;
};
```

### 步骤 2: 在 ConfigurePorts() 中注册端口

在 `src/application/container/ApplicationContainer.cpp` 的 `ConfigurePorts()` 方法中：

```cpp
void ApplicationContainer::ConfigurePorts() {
    // ... 现有端口注册 ...

    // 11. 系统时钟适配器
    clock_port_ = std::make_shared<Infrastructure::Adapters::System::SystemClockAdapter>();
    RegisterPort<Domain::Ports::IClockPort>(clock_port_);

    // 12. 环境信息提供者适配器
    env_info_port_ = std::make_shared<Infrastructure::Adapters::System::EnvironmentInfoAdapter>(
        "1.0.0",  // build_version
        "",       // git_commit_hash（可选）
        "",       // git_branch（可选）
        false     // git_dirty（可选）
    );
    RegisterPort<Domain::Ports::IEnvironmentInfoProviderPort>(env_info_port_);

    // 13. 栈追踪提供者适配器（条件编译）
    #ifdef SILIGEN_BUILD_DEBUGGING
    stacktrace_port_ = std::make_shared<Infrastructure::Adapters::Debugging::BoostStacktraceAdapter>(
        64,   // max_frames
        true  // thread_safe
    );
    RegisterPort<Domain::Ports::IStacktraceProviderPort>(stacktrace_port_);
    #endif
}
```

### 步骤 3: 注册 BugRecorderService

在 `ConfigureUseCases()` 方法中：

```cpp
void ApplicationContainer::ConfigureUseCases() {
    // ... 现有用例注册 ...

    // BugRecorderService
    bug_recorder_service_ = std::make_shared<UseCases::BugRecorderService>(
        bug_store_port_,
        clock_port_,
        stacktrace_port_,
        env_info_port_
    );

    RegisterSingleton<UseCases::BugRecorderService>(bug_recorder_service_);

    // ErrorHandlingUseCase
    error_handling_use_case_ = std::make_shared<UseCases::ErrorHandlingUseCase>(
        bug_recorder_service_
    );

    RegisterSingleton<UseCases::ErrorHandlingUseCase>(error_handling_use_case_);
}
```

### 步骤 4: 更新 CMakeLists.txt

在 `src/infrastructure/CMakeLists.txt` 中添加：

```cmake
add_library(siligen_services STATIC
    services/CMPTestPresetService.cpp
    adapters/parsing/DXFFileParsingAdapter.cpp
    adapters/system/storage/files/LocalFileStorageAdapter.cpp
    handlers/GlobalExceptionHandler.cpp  # Phase 2.3: 全局异常处理器
)
```

---

## 集成到 main.cpp

### 基础集成

```cpp
#include "application/container/ApplicationContainer.h"
#include "application/usecases/ErrorHandlingUseCase.h"
#include "infrastructure/handlers/GlobalExceptionHandler.h"
#include <iostream>

int main() {
    // 1. 初始化容器
    ApplicationContainer container("config/machine_config.ini");
    container.Configure();

    // 2. 解析服务
    auto error_handling = container.Resolve<ErrorHandlingUseCase>();
    auto bug_recorder = container.Resolve<BugRecorderService>();

    // 3. 安装全局异常处理器
    GlobalExceptionHandler handler(bug_recorder);
    handler.Install();

    // 4. 使用 ErrorHandlingUseCase 包装关键操作
    auto result = error_handling->ExecuteWithErrorHandling([&]() {
        // 初始化系统
        auto init = container.Resolve<InitializeSystemUseCase>();
        return init->Execute();
    }, "InitializeSystem");

    if (result.IsFailure()) {
        std::cerr << "初始化失败: " << result.GetError().GetMessage() << std::endl;
        return 1;
    }

    // ... 主程序逻辑 ...

    return 0;
}
```

### 完整示例：CLI 应用

```cpp
#include "application/container/ApplicationContainer.h"
#include "application/usecases/ErrorHandlingUseCase.h"
#include "infrastructure/handlers/GlobalExceptionHandler.h"
#include <iostream>

class Application {
public:
    Application() {
        // 初始化容器
        container_ = std::make_unique<ApplicationContainer>("config/machine_config.ini");
        container_->Configure();

        // 解析服务
        error_handling_ = container_->Resolve<ErrorHandlingUseCase>();
        bug_recorder_ = container_->Resolve<BugRecorderService>();

        // 安装全局异常处理器
        exception_handler_ = std::make_unique<GlobalExceptionHandler>(bug_recorder_);
        exception_handler_->Install();
    }

    ~Application() {
        exception_handler_->Uninstall();
    }

    int Run() {
        // 初始化
        auto init_result = error_handling_->ExecuteWithErrorHandling([&]() {
            return Initialize();
        }, "ApplicationInitialize");

        if (init_result.IsFailure()) {
            std::cerr << "初始化失败" << std::endl;
            return 1;
        }

        // 运行主循环
        auto run_result = error_handling_->ExecuteWithErrorHandling([&]() {
            return RunMainLoop();
        }, "MainLoop");

        if (run_result.IsFailure()) {
            std::cerr << "运行失败" << std::endl;
            return 1;
        }

        return 0;
    }

private:
    Result<void> Initialize() {
        // 初始化逻辑
        return Result<void>::Ok();
    }

    Result<void> RunMainLoop() {
        // 主循环逻辑
        return Result<void>::Ok();
    }

    std::unique_ptr<ApplicationContainer> container_;
    std::shared_ptr<ErrorHandlingUseCase> error_handling_;
    std::shared_ptr<BugRecorderService> bug_recorder_;
    std::unique_ptr<GlobalExceptionHandler> exception_handler_;
};

int main() {
    Application app;
    return app.Run();
}
```

---

## CMake 配置

### 启用调试符号

在根目录 `CMakeLists.txt` 中：

```cmake
# Bug 检测系统 - Release 调试符号配置
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Zi")
set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /DEBUG")

set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /Zi /DNDEBUG")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG:FASTLINK")
```

### 启用 Boost.Stacktrace

```bash
cmake -B build -G Ninja -DSILIGEN_BUILD_DEBUGGING=ON
cmake --build build
```

---

## 架构合规性检查

### 验证依赖方向

```bash
# 检查 Domain 层不依赖 Infrastructure 层
rg "#include.*infrastructure" src/domain/
# 预期: 无匹配

# 检查 Application 层不直接依赖 Infrastructure 层适配器
rg "#include.*infrastructure/adapters" src/application/
# 预期: 无匹配（除了 GlobalExceptionHandler，位于 Infrastructure 层）
```

### 验证依赖注入

```bash
# 检查 Infrastructure 层不使用单例访问 Application 层
rg "ApplicationContainer::GetInstance" src/infrastructure/
# 预期: 无匹配

# 检查 ErrorHandlingUseCase 在 Application 层
rg "class ErrorHandlingUseCase" src/
# 预期: src/application/usecases/ErrorHandlingUseCase.h
```

### 验证 Domain 层纯净性

```bash
# 检查 Domain 层无 IO 操作
rg "std::(cout|thread)" src/domain/
# 预期: 无匹配

# 检查 Domain 层无自动捕获逻辑
rg "ErrorHandlingUseCase|BugRecorderService" src/domain/
# 预期: 无匹配
```

---

## 故障排查

### 问题 1: 编译错误 - 找不到 Boost.Stacktrace

**错误信息:**
```
fatal error: boost/stacktrace.hpp: No such file or directory
```

**解决方案:**
1. 确认 Boost 版本 >= 1.64
2. 启用 `SILIGEN_BUILD_DEBUGGING=ON`
3. 安装 Boost.Stacktrace 依赖

```bash
# Windows (vcpkg)
vcpkg install boost-stacktrace-basic:x64-windows
vcpkg install boost-stacktrace-windbg:x64-windows
```

### 问题 2: 链接错误 - 未定义的符号

**错误信息:**
```
undefined reference to `GlobalExceptionHandler::GetInstance()'
```

**解决方案:**
确认 `GlobalExceptionHandler.cpp` 已添加到 `CMakeLists.txt`:

```cmake
add_library(siligen_services STATIC
    # ...
    handlers/GlobalExceptionHandler.cpp
)
```

### 问题 3: 运行时错误 - Bug 报告未生成

**解决方案:**
1. 检查 `bugs/` 目录权限
2. 验证端口是否正确注册
3. 查看控制台错误输出

```cpp
// 添加调试代码
auto bug_recorder = container.Resolve<BugRecorderService>();
if (bug_recorder == nullptr) {
    std::cerr << "[ERROR] BugRecorderService 未注册" << std::endl;
}
```

---

## 性能考虑

### 栈捕获开销

- **Debug 模式:** ~50ms (64 帧)
- **Release 模式:** ~5ms (64 帧)

**建议:**
- 热路径中禁用自动栈捕获
- 使用采样策略（仅捕获 10% 的失败）

### Bug 报告序列化开销

- **JSON 序列化:** ~1ms
- **文件写入:** ~5ms

**建议:**
- 批量提交 Bug 报告
- 异步保存（未来版本）

---

## 参考资料

- **使用指南:** `docs/06-user-guides/bug-detection-system-guide.md`
- **架构文档:** `docs/01-architecture/bug-detection-system-architecture.md`
- **API 参考:** `docs/02-api/bug-detection-api.md`

---

**文档维护:** 请在修改集成流程时同步更新本文档

**反馈:** 如有问题，请提交 Issue 或联系开发团队
