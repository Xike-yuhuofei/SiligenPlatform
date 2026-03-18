# Bug 检测系统使用指南

**版本:** 1.0.0
**最后更新:** 2026-01-09
**适用人群:** 应用层开发者

---

## 概述

Bug 检测系统是一个自动化的错误记录和崩溃报告系统，基于六边形架构原则设计。

### 主要功能

- ✅ 自动捕获操作失败并记录 Bug
- ✅ 捕获系统崩溃和未处理异常
- ✅ 生成包含栈追踪的详细 Bug 报告
- ✅ 支持同步和异步操作
- ✅ 零依赖侵入，通过依赖注入集成

### 架构合规性

- ✅ 符合六边形架构原则
- ✅ Application 层捕获，Domain 层纯净
- ✅ 使用依赖注入，无单例模式
- ✅ 线程安全

---

## 快速开始

### 1. 基础使用

```cpp
#include "application/usecases/ErrorHandlingUseCase.h"
#include "application/container/ApplicationContainer.h"

int main() {
    // 1. 创建容器
    ApplicationContainer container("config/machine_config.ini");
    container.Configure();

    // 2. 解析 ErrorHandlingUseCase
    auto error_handling = container.Resolve<ErrorHandlingUseCase>();

    // 3. 包装操作
    auto result = error_handling->ExecuteWithErrorHandling([&]() {
        // 您的业务逻辑
        return SomeUseCase->Execute();
    }, "SomeOperation");

    if (result.IsFailure()) {
        // 失败已自动记录到 Bug 存储系统
        std::cerr << "操作失败: " << result.GetError().GetMessage() << std::endl;
    }

    return 0;
}
```

### 2. 异步操作

```cpp
// 异步执行错误处理
auto future_result = error_handling->ExecuteAsyncWithErrorHandling([&]() {
    // 您的异步业务逻辑
    return AsyncOperation->Execute();
}, "AsyncOperation");

// 等待结果
auto result = future_result.get();
```

### 3. 安装全局异常处理器

```cpp
#include "infrastructure/handlers/GlobalExceptionHandler.h"

int main() {
    ApplicationContainer container("config/machine_config.ini");
    container.Configure();

    // 解析 BugRecorderService
    auto bug_recorder = container.Resolve<BugRecorderService>();

    // 安装全局异常处理器
    GlobalExceptionHandler handler(bug_recorder);
    handler.Install();

    // ... 应用逻辑 ...

    return 0;
}
```

---

## 常见场景

### 场景 1: 关键操作重试

```cpp
auto error_handling = container.Resolve<ErrorHandlingUseCase>();

int max_retries = 3;
for (int i = 0; i < max_retries; i++) {
    auto result = error_handling->ExecuteWithErrorHandling([&]() {
        return CriticalOperation->Execute();
    }, "CriticalOperation");

    if (result.IsSuccess()) {
        // 成功，退出重试循环
        break;
    }

    std::cout << "操作失败，重试 " << (i + 1) << "/" << max_retries << std::endl;
}
```

### 场景 2: 批量操作错误收集

```cpp
auto error_handling = container.Resolve<ErrorHandlingUseCase>();

std::vector<std::string> items = {"item1", "item2", "item3"};
int success_count = 0;

for (const auto& item : items) {
    auto result = error_handling->ExecuteWithErrorHandling([&]() {
        return ProcessItem(item);
    }, "ProcessItem_" + item);

    if (result.IsSuccess()) {
        success_count++;
    }
}

std::cout << "成功处理 " << success_count << "/" << items.size() << " 个项目" << std::endl;
```

### 场景 3: 多线程错误处理

```cpp
auto error_handling = container.Resolve<ErrorHandlingUseCase>();

std::vector<std::thread> threads;
for (int i = 0; i < 4; i++) {
    threads.emplace_back([error_handling, i]() {
        auto result = error_handling->ExecuteWithErrorHandling([&]() {
            return ThreadSafeOperation(i);
        }, "ThreadOperation_" + std::to_string(i));

        if (result.IsFailure()) {
            // 失败已自动记录
        }
    });
}

for (auto& thread : threads) {
    thread.join();
}
```

---

## 配置选项

### Bug 存储位置

默认存储在 `bugs/` 目录，可在 `ApplicationContainer` 中配置：

```cpp
// 在 ApplicationContainer.cpp 的 ConfigurePorts() 中修改
bug_store_port_ = std::make_shared<FileBugStoreAdapter>("/path/to/bug/storage");
```

### 栈追踪深度

可在创建 BoostStacktraceAdapter 时配置：

```cpp
stacktrace_port_ = std::make_shared<BoostStacktraceAdapter>(
    128,  // 最大栈帧数
    true  // 启用线程安全
);
```

---

## Bug 报告格式

每个 Bug 报告包含以下字段：

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | string | Bug 唯一标识符 |
| `timestamp` | int64_t | Unix 时间戳（毫秒） |
| `title` | string | Bug 标题 |
| `message` | string | Bug 详细描述 |
| `severity` | BugSeverity | 严重级别（Info/Warning/Error/Critical） |
| `stacktrace` | vector\<string\> | 栈追踪 |
| `environment_info` | EnvironmentInfo | 环境信息 |
| `context` | map\<string, string\> | 额外上下文 |

### 示例 Bug 报告

```json
{
  "id": "bug_20260109_123456_abc123",
  "timestamp": 1736423746000,
  "title": "Operation Failed: CriticalOperation",
  "message": "Connection timeout",
  "severity": "Error",
  "stacktrace": [
    "frame #0: CriticalOperation::Execute() at critical.cpp:42",
    "frame #1: main() at main.cpp:15"
  ],
  "environment_info": {
    "build_version": "1.0.0",
    "os": "Windows",
    "architecture": "x86_64"
  },
  "context": {
    "operation": "CriticalOperation",
    "error_code": "1001",
    "error_message": "Connection timeout"
  }
}
```

---

## 最佳实践

### ✅ 推荐做法

1. **始终包装关键操作**
   ```cpp
   auto result = error_handling->ExecuteWithErrorHandling([&]() {
       return CriticalOperation();
   }, "CriticalOperation");
   ```

2. **提供清晰的操作名称**
   ```cpp
   // ✅ 好
   "InitializeHardwareController"

   // ❌ 不好
   "op1"
   ```

3. **检查返回值**
   ```cpp
   auto result = error_handling->ExecuteWithErrorHandling(...);
   if (result.IsFailure()) {
       // 处理失败（已自动记录）
   }
   ```

4. **安装全局异常处理器**
   ```cpp
   GlobalExceptionHandler handler(bug_recorder);
   handler.Install();
   ```

### ❌ 避免做法

1. **不要在热路径中使用**
   ```cpp
   // ❌ 不好：高频操作
   for (int i = 0; i < 1000000; i++) {
       error_handling->ExecuteWithErrorHandling(...);
   }
   ```

2. **不要忽略返回值**
   ```cpp
   // ❌ 不好：未检查返回值
   error_handling->ExecuteWithErrorHandling(...);
   ```

3. **不要在 Domain 层使用**
   ```cpp
   // ❌ 违规：Domain 层不应依赖 Application 层
   // src/domain/**.cpp
   error_handling->ExecuteWithErrorHandling(...);
   ```

---

## 故障排查

### 问题：Bug 报告未生成

**解决方案：**

1. 检查 `bugs/` 目录是否存在且有写权限
2. 查看控制台是否有错误信息
3. 验证 BugRecorderService 是否正确解析

```cpp
auto bug_recorder = container.Resolve<BugRecorderService>();
if (bug_recorder == nullptr) {
    std::cerr << "[ERROR] BugRecorderService 未正确注册" << std::endl;
}
```

### 问题：栈追踪为空

**解决方案：**

1. 确认编译时启用了 `SILIGEN_BUILD_DEBUGGING=ON`
2. 检查 Release 模式下是否配置了调试符号 `/Zi`
3. 验证 Boost.Stacktrace 库是否正确链接

```bash
cmake -B build -G Ninja -DSILIGEN_BUILD_DEBUGGING=ON
cmake --build build
```

---

## 参考资料

- **架构文档:** `docs/01-architecture/bug-detection-system-architecture.md`
- **集成指南:** `docs/04-development/bug-detection-integration-guide.md`
- **API 文档:** `docs/02-api/bug-detection-api.md`

---

**文档维护:** 请在修改系统功能时同步更新本文档

**反馈:** 如有问题，请提交 Issue 或联系开发团队
