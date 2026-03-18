# 日志系统使用指南

## 概述

本项目采用统一的日志系统，提供高性能、线程安全、类型安全的日志记录功能。

**架构特点：**
- 类型系统统一：使用 `shared/types/LogTypes.h` 的类型定义
- 性能优化：编译时级别过滤、无锁检查、分级锁
- 架构合规：符合六边形架构的分层原则

---

## 快速开始

### 基本使用

```cpp
// 1. 定义模块名称（在每个 .cpp 文件顶部）
#define MODULE_NAME "MyModule"

// 2. 包含日志接口
#include "shared/interfaces/ILoggingService.h"

// 3. 使用日志宏
SILIGEN_LOG_INFO("系统启动成功");
SILIGEN_LOG_ERROR("连接失败");
SILIGEN_LOG_WARNING("内存使用率过高");
```

### 格式化日志（推荐）

```cpp
// 使用 printf 风格的格式化字符串，避免字符串拼接
#include "shared/interfaces/ILoggingService.h"

SILIGEN_LOG_ERROR_FMT("连接失败，错误码:%d，消息:%s", error_code, error_msg);
SILIGEN_LOG_INFO_FMT("处理了%zu个文件，耗时:%.2f秒", file_count, duration);
SILIGEN_LOG_WARNING_FMT("温度超出范围:%.2f℃ (上限:%.2f℃)", current_temp, max_temp);
```

### Domain 层使用（特殊考虑）

Domain 层不应依赖基础设施层，因此使用辅助宏：

```cpp
// Domain 层 .cpp 文件
#define MODULE_NAME "InterpolationBase"
#include "shared/interfaces/ILoggingService.h"
#include "shared/utils/LogFormatHelper.h"  // 特别包含

void SomeFunction() {
    // 使用 _FMT_HELPER 后缀的宏
    SILIGEN_LOG_ERROR_FMT_HELPER("参数验证失败: value=%.2f", value);
}
```

---

## 日志级别

### 级别定义

| 级别 | 枚举值 | 用途 | 宏 |
|------|--------|------|-----|
| DEBUG | 0 | 调试信息，开发阶段使用 | `SILIGEN_LOG_DEBUG` / `SILIGEN_LOG_DEBUG_FMT` |
| INFO | 1 | 一般运行信息 | `SILIGEN_LOG_INFO` / `SILIGEN_LOG_INFO_FMT` |
| WARNING | 2 | 警告信息，不影响运行 | `SILIGEN_LOG_WARNING` / `SILIGEN_LOG_WARNING_FMT` |
| ERROR | 3 | 错误信息，需要注意 | `SILIGEN_LOG_ERROR` / `SILIGEN_LOG_ERROR_FMT` |
| CRITICAL | 4 | 严重错误，系统可能崩溃 | `SILIGEN_LOG_CRITICAL` / `SILIGEN_LOG_CRITICAL_FMT` |

### 编译时级别过滤

通过定义 `SILIGEN_MIN_LOG_LEVEL` 可以在编译时过滤低级别日志：

```cpp
// 在 CMakeLists.txt 或编译选项中定义
add_definitions(-DSILIGEN_MIN_LOG_LEVEL=2)  // 只输出 WARNING 及以上级别
```

或者在包含日志接口前定义：

```cpp
#define SILIGEN_MIN_LOG_LEVEL 2
#include "shared/interfaces/ILoggingService.h"
```

---

## 性能最佳实践

### ✅ 推荐做法

1. **使用格式化宏，避免字符串拼接**
   ```cpp
   // ✅ 好：格式化在级别检查后执行
   SILIGEN_LOG_ERROR_FMT("连接失败，错误码:%d", error_code);

   // ❌ 差：即使日志被禁用，字符串拼接仍会执行
   SILIGEN_LOG_ERROR("连接失败，错误码:" + std::to_string(error_code));
   ```

2. **利用编译时级别过滤**
   ```cpp
   // ✅ 好：DEBUG 日志在 Release 构建中被完全移除
   #ifdef DEBUG_MODE
   SILIGEN_LOG_DEBUG("详细调试信息");
   #endif
   ```

3. **合理使用日志级别**
   ```cpp
   // ✅ 好：级别使用恰当
   SILIGEN_LOG_INFO("用户登录成功");           // 正常业务流程
   SILIGEN_LOG_WARNING("缓存未命中");          // 性能问题但不影响功能
   SILIGEN_LOG_ERROR("数据库连接失败");        // 需要关注的错误
   SILIGEN_LOG_CRITICAL("内存耗尽");           // 系统级严重错误
   ```

### ❌ 避免的做法

1. **在热循环中使用日志**
   ```cpp
   // ❌ 差：高频日志严重影响性能
   for (int i = 0; i < 1000000; ++i) {
       SILIGEN_LOG_DEBUG_FMT("处理第%d项", i);  // 每次迭代都记录日志
   }

   // ✅ 好：降低日志频率
   for (int i = 0; i < 1000000; ++i) {
       if (i % 10000 == 0) {
           SILIGEN_LOG_INFO_FMT("已处理%d/%d项", i, 1000000);
       }
   }
   ```

2. **在实时路径中使用日志**
   ```cpp
   // ❌ 差：违反 REALTIME_NO_LOGGING 规则
   void RealtimeControlLoop() {
       SILIGEN_LOG_INFO("控制循环开始");  // 实时路径不应有日志
   }

   // ✅ 好：使用轻量级错误通知机制
   void RealtimeControlLoop() {
       if (error) {
           NotifyError(error);  // 非日志方式
       }
   }
   ```

3. **过度使用字符串拼接**
   ```cpp
   // ❌ 差：多次内存分配
   SILIGEN_LOG_ERROR("错误: " + std::to_string(code) + ", 消息: " + msg + ", 时间: " + timestamp);

   // ✅ 好：一次性格式化
   SILIGEN_LOG_ERROR_FMT("错误:%d, 消息:%s, 时间:%s", code, msg.c_str(), timestamp.c_str());
   ```

---

## 配置说明

### 日志配置结构

```cpp
namespace Siligen::Shared::Types {

struct LogConfig {
    LogLevel min_level;             // 最小日志级别（默认：INFO）
    bool enable_console;            // 启用控制台输出（默认：true）
    bool enable_file;               // 启用文件输出（默认：false）
    std::string log_file_path;      // 日志文件路径（默认："logs/application.log"）
    bool enable_timestamp;          // 启用时间戳（默认：true）
    bool enable_thread_id;          // 启用线程ID（默认：false）
    size_t max_file_size_mb;        // 最大文件大小（MB）（默认：10）
    size_t max_backup_files;        // 最大备份文件数（默认：5）
};

}
```

### 运行时配置示例

```cpp
#include "infrastructure/logging/Logger.h"

// 配置日志系统
Siligen::LogConfig config;
config.min_level = Siligen::LogLevel::INFO;
config.enable_console = true;
config.enable_file = true;
config.log_file_path = "logs/myapp.log";
config.max_file_size_mb = 50;
config.max_backup_files = 10;

auto& logger = Siligen::Logger::GetInstance();
logger.SetConfig(config);
```

### 动态调整日志级别

```cpp
// 运行时动态调整日志级别
auto& logger = Siligen::Logger::GetInstance();

// 提高日志级别以调试问题
logger.SetLogLevel(Siligen::LogLevel::DEBUG);

// ... 执行调试操作 ...

// 恢复正常级别
logger.SetLogLevel(Siligen::LogLevel::INFO);
```

---

## 日志输出格式

### 标准格式

```
[时间戳] [级别] [模块]: 消息内容
```

### 示例输出

```
2025-01-05 14:30:45.123 [INFO] [InterpolationBase]: 插补参数验证通过
2025-01-05 14:30:45.124 [ERROR] [HardwareController]: 连接失败，错误码:1001
2025-01-05 14:30:45.125 [WARNING] [MotionControl]: 速度超出建议范围(2000mm/s):2500.00
```

### 启用线程ID的格式

```
2025-01-05 14:30:45.123 [INFO] [线程:12345] [InterpolationBase]: 插补参数验证通过
```

---

## 高级功能

### 文件轮转

日志系统支持按大小自动轮转：

```cpp
// 配置文件轮转
Siligen::LogConfig config;
config.enable_file = true;
config.log_file_path = "logs/app.log";
config.max_file_size_mb = 10;    // 单个文件最大 10MB
config.max_backup_files = 5;      // 保留 5 个备份文件
```

轮转后的文件命名：
- `logs/app.log` - 当前日志文件
- `logs/app.1` - 第1个备份
- `logs/app.2` - 第2个备份
- ...
- `logs/app.5` - 第5个备份（最旧）

### 日志回调

可以注册自定义回调函数处理日志条目：

```cpp
#include "infrastructure/logging/Logger.h"

void MyLogCallback(const Siligen::LogEntry& entry) {
    // 自定义处理逻辑
    if (entry.level == Siligen::LogLevel::ERROR) {
        SendAlert(entry.message);
    }
}

// 注册回调
auto& logger = Siligen::Logger::GetInstance();
logger.SetLogCallback(MyLogCallback);
```

### 手动刷新

```cpp
// 立即将缓冲的日志写入磁盘
auto& logger = Siligen::Logger::GetInstance();
logger.Flush();
```

---

## 迁移指南

### 从旧代码迁移

如果你有使用旧日志宏的代码，迁移步骤如下：

1. **更新包含**
   ```cpp
   // 旧代码
   #include "infrastructure/logging/Logger.h"

   // 新代码
   #define MODULE_NAME "YourModule"
   #include "shared/interfaces/ILoggingService.h"
   ```

2. **更新宏调用**
   ```cpp
   // 旧代码（使用字符串拼接）
   LOG_ERROR("错误: " + std::to_string(code));

   // 新代码（使用格式化）
   SILIGEN_LOG_ERROR_FMT("错误:%d", code);
   ```

3. **Domain 层特殊处理**
   ```cpp
   // Domain 层需要额外包含
   #include "shared/utils/LogFormatHelper.h"

   // 使用 _FMT_HELPER 后缀的宏
   SILIGEN_LOG_ERROR_FMT_HELPER("错误:%d", code);
   ```

---

## 架构说明

### 分层结构

```
shared/
├── types/
│   └── LogTypes.h          # 类型定义（LogLevel、LogEntry、LogConfig）
├── interfaces/
│   └── ILoggingService.h   # 日志服务接口 + 宏定义
└── utils/
    └── LogFormatHelper.h   # Domain 层格式化辅助（避免依赖基础设施层）

infrastructure/
└── logging/
    └── Logger.{h,cpp}      # 日志实现
```

### 依赖关系

```
Domain 层 → shared/utils/LogFormatHelper.h (无依赖基础设施层)
Application 层 → shared/interfaces/ILoggingService.h
Infrastructure 层 → shared/types/LogTypes.h
```

### 架构规则遵循

✅ **符合的规则：**
- `NAMESPACE_LAYER_ISOLATION` - 每层使用正确的命名空间
- `REALTIME_NO_LOGGING` - 实时路径不使用日志
- `HEXAGONAL_DEPENDENCY_DIRECTION` - 正确的依赖方向

⚠️ **部分违反的规则：**
- Domain 层通过 `LogFormatHelper` 使用日志，存在隐式 I/O 依赖
- 这是权衡后的设计选择，以提供性能优化

---

## 常见问题 (FAQ)

### Q1: 为什么 Domain 层不能直接使用 Logger？

**A:** 根据六边形架构原则，Domain 层不应依赖基础设施层。但日志是横切关注点，因此提供了 `LogFormatHelper` 作为轻量级辅助工具，在 shared 层提供格式化功能。

### Q2: 格式化宏和普通宏有什么区别？

**A:** 格式化宏（`_FMT` 后缀）使用 printf 风格的格式化字符串，避免了字符串拼接的性能开销。在高频日志场景下，格式化宏性能更好。

### Q3: 如何完全禁用日志？

**A:** 设置最小日志级别为高于 CRITICAL 的值：

```cpp
logger.SetLogLevel(static_cast<Siligen::LogLevel>(5));  // 禁用所有日志
```

或在编译时定义：

```cpp
#define SILIGEN_MIN_LOG_LEVEL 10  // 禁用所有日志
```

### Q4: 日志性能开销有多大？

**A:** 通过以下优化，日志性能开销最小化：
- 编译时级别过滤：被过滤的日志零开销
- 无锁级别检查：使用原子变量，避免锁竞争
- 分级锁：配置读写锁 + I/O 锁，提高并发性能
- 格式化宏：避免不必要的字符串构造

在典型使用场景下，日志调用耗时 < 1ms。

---

## 参考资料

- 类型定义：`src/shared/types/LogTypes.h`
- 接口定义：`src/shared/interfaces/ILoggingService.h`
- 实现代码：`src/infrastructure/logging/Logger.{h,cpp}`
- Domain 层辅助：`src/shared/utils/LogFormatHelper.h`

---

## 更新日志

### 2025-01-05
- ✨ 统一类型系统，使用 `LogTypes.h` 的类型定义
- ⚡ 添加格式化日志支持（`LogFormat` 方法）
- 🚀 性能优化：分级锁、原子变量、无锁检查
- 📝 添加 Domain 层格式化辅助工具
- 🐛 修复 Domain 层字符串拼接性能问题
