// ILoggingService.h
// 版本: 1.0.0
// 描述: 日志服务接口契约 - 定义统一日志记录的标准接口
//
// 六边形架构 - 端口层接口
// 依赖方向: 所有层都可以依赖此接口进行日志记录

#pragma once

#include "../types/LogTypes.h"
#include "../types/Result.h"
#include "../util/ModuleDependencyCheck.h"
#include "../di/LoggingServiceLocator.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Siligen::Shared::Interfaces {

/// @brief 日志服务接口
/// @details 定义统一的日志记录接口,解决LOG_INFO/LOG_ERROR/LOG_DEBUG宏冲突问题
///
/// 性能约束:
/// - 最大响应时间: 1ms
/// - 内存开销限制: 512B
/// - 线程安全: 必须支持多线程并发调用
/// - 无异常抛出: 禁止异常,使用优雅降级处理
///
/// 输出格式要求:
/// - 结构化格式: [LEVEL] 模块名称: 消息内容
/// - 示例: [ERROR] HardwareController: 连接失败 (错误码: 1001)
///
/// 使用示例:
/// @code
/// auto logger = ServiceLocator::Get<ILoggingService>();
/// logger->LogInfo("系统启动成功", "System");
/// logger->LogError("硬件连接失败 (1001)", "HardwareController");
/// @endcode
///
/// 宏定义使用:
/// @code
/// #define SILIGEN_LOG_INFO(message) \
///     ServiceLocator::Get<ILoggingService>()->LogInfo(message, MODULE_NAME)
/// @endcode
class ILoggingService : public Siligen::Shared::Util::SharedModuleTag {
   public:
    virtual ~ILoggingService() = default;

    // ============================================================
    // 基础日志记录接口
    // ============================================================

    /// @brief 记录日志
    /// @param level 日志级别
    /// @param message 日志消息 (最大1000字符)
    /// @param category 日志分类/模块名称 (最大100字符,默认"General")
    /// @return Result<void> 记录结果
    /// @note 线程安全,无异常抛出
    virtual Siligen::Shared::Types::Result<void> Log(Siligen::Shared::Types::LogLevel level,
                                                     const std::string& message,
                                                     const std::string& category = "General") = 0;

    /// @brief 记录调试日志
    /// @param message 日志消息
    /// @param category 日志分类/模块名称
    /// @return Result<void> 记录结果
    /// @note 输出格式: [DEBUG] 模块名称: 消息内容
    virtual Siligen::Shared::Types::Result<void> LogDebug(const std::string& message,
                                                          const std::string& category = "General") = 0;

    /// @brief 记录信息日志
    /// @param message 日志消息
    /// @param category 日志分类/模块名称
    /// @return Result<void> 记录结果
    /// @note 输出格式: [INFO] 模块名称: 消息内容
    virtual Siligen::Shared::Types::Result<void> LogInfo(const std::string& message,
                                                         const std::string& category = "General") = 0;

    /// @brief 记录警告日志
    /// @param message 日志消息
    /// @param category 日志分类/模块名称
    /// @return Result<void> 记录结果
    /// @note 输出格式: [WARNING] 模块名称: 消息内容
    virtual Siligen::Shared::Types::Result<void> LogWarning(const std::string& message,
                                                            const std::string& category = "General") = 0;

    /// @brief 记录错误日志
    /// @param message 日志消息
    /// @param category 日志分类/模块名称
    /// @return Result<void> 记录结果
    /// @note 输出格式: [ERROR] 模块名称: 消息内容
    virtual Siligen::Shared::Types::Result<void> LogError(const std::string& message,
                                                          const std::string& category = "General") = 0;

    /// @brief 记录严重错误日志
    /// @param message 日志消息
    /// @param category 日志分类/模块名称
    /// @return Result<void> 记录结果
    /// @note 输出格式: [CRITICAL] 模块名称: 消息内容
    virtual Siligen::Shared::Types::Result<void> LogCritical(const std::string& message,
                                                             const std::string& category = "General") = 0;

    // ============================================================
    // 配置管理接口
    // ============================================================

    /// @brief 设置日志级别
    /// @param level 最小日志级别
    /// @return Result<void> 设置结果
    /// @note 低于此级别的日志将被过滤
    virtual Siligen::Shared::Types::Result<void> SetLogLevel(Siligen::Shared::Types::LogLevel level) = 0;

    /// @brief 获取当前日志级别
    /// @return Result<LogLevel> 当前日志级别
    virtual Siligen::Shared::Types::Result<Siligen::Shared::Types::LogLevel> GetLogLevel() const = 0;

    /// @brief 配置日志服务
    /// @param config 日志配置信息
    /// @return Result<void> 配置结果
    /// @note 包含输出格式、目标、文件路径、轮转策略等
    virtual Siligen::Shared::Types::Result<void> Configure(const Siligen::Shared::Types::LogConfiguration& config) = 0;

    /// @brief 获取当前配置
    /// @return Result<LogConfiguration> 当前配置信息
    virtual Siligen::Shared::Types::Result<Siligen::Shared::Types::LogConfiguration> GetConfiguration() const = 0;

    // ============================================================
    // 高级功能接口
    // ============================================================

    /// @brief 带上下文的日志记录
    /// @param level 日志级别
    /// @param message 日志消息
    /// @param category 日志分类/模块名称
    /// @param source_file 源文件名
    /// @param source_line 源文件行号
    /// @return Result<void> 记录结果
    /// @note 用于宏定义中包含文件名和行号信息
    virtual Siligen::Shared::Types::Result<void> LogWithContext(Siligen::Shared::Types::LogLevel level,
                                                                const std::string& message,
                                                                const std::string& category,
                                                                const std::string& source_file,
                                                                int32_t source_line) = 0;

    /// @brief 获取最近的日志条目
    /// @param count 返回条目数量 (1-1000,默认100)
    /// @param min_level 最小日志级别过滤 (可选)
    /// @param category_filter 分类过滤器 (可选)
    /// @return Result<vector<LogEntry>> 日志条目列表
    /// @note 用于调试和日志查看
    virtual Siligen::Shared::Types::Result<std::vector<Siligen::Shared::Types::LogEntry>> GetRecentLogs(
        int32_t count = 100,
        Siligen::Shared::Types::LogLevel min_level = static_cast<Siligen::Shared::Types::LogLevel>(0),
        const std::string& category_filter = "") = 0;

    /// @brief 刷新日志缓冲区
    /// @return Result<void> 刷新结果
    /// @note 立即将所有缓冲的日志写入磁盘
    virtual Siligen::Shared::Types::Result<void> Flush() = 0;

    // ============================================================
    // 测试支持接口
    // ============================================================

    /// @brief 设置日志回调（用于测试）
    /// @param callback 日志条目回调函数
    /// @note 每次记录日志时调用，用于测试验证日志内容
    virtual void SetLogCallback(std::function<void(const Siligen::Shared::Types::LogEntry&)> callback) = 0;

    /// @brief 清除日志回调
    /// @note 移除之前设置的回调函数
    virtual void ClearLogCallback() = 0;

    /// @brief 带属性的日志记录（支持结构化数据）
    /// @param level 日志级别
    /// @param message 日志消息
    /// @param category 分类
    /// @param attributes 结构化属性（键值对）
    /// @return Result<void> 记录结果
    /// @note 属性将被附加到消息末尾，格式为 "message {key1=value1, key2=value2}"
    virtual Siligen::Shared::Types::Result<void> LogWithAttributes(
        Siligen::Shared::Types::LogLevel level,
        const std::string& message,
        const std::string& category,
        const std::map<std::string, std::string>& attributes) = 0;

    // ============================================================
    // 接口元数据
    // ============================================================

    /// @brief 获取接口版本号
    /// @return 版本字符串 (语义化版本)
    virtual const char* GetInterfaceVersion() const noexcept {
        return "1.0.0";
    }

    /// @brief 获取实现类型名称
    /// @return 实现类型名称
    /// @note 用于调试和日志记录
    virtual const char* GetImplementationType() const noexcept = 0;

    /// @brief 检查是否启用了指定日志级别
    /// @param level 要检查的日志级别
    /// @return true=启用, false=禁用
    /// @note 用于避免不必要的字符串构造
    virtual bool IsLevelEnabled(Siligen::Shared::Types::LogLevel level) const noexcept = 0;

    /// @brief 获取全局日志服务实例 (Service Locator Pattern)
    /// @return 日志服务实例指针 (可能为nullptr if not initialized)
    /// @note 通过 LoggingServiceLocator 获取实例
    static ILoggingService* GetInstance() {
        auto service = DI::LoggingServiceLocator::GetInstance().GetService();
        return service.get();
    }
};

}  // namespace Siligen::Shared::Interfaces

// ============================================================
// 统一日志宏定义 - 解决LOG_INFO/LOG_ERROR/LOG_DEBUG冲突
// ============================================================

// 宏定义使用MODULE_NAME变量,需在包含此头文件前定义
// 示例: #define MODULE_NAME "HardwareController"

#ifndef MODULE_NAME
#define MODULE_NAME "Unknown"
#endif

// 编译时日志级别过滤,提升性能
#ifndef SILIGEN_MIN_LOG_LEVEL
#define SILIGEN_MIN_LOG_LEVEL 0  // 默认允许所有级别
#endif

// 日志宏定义 - 使用SILIGEN_前缀避免冲突
#define SILIGEN_LOG_DEBUG(message)                                                     \
    do {                                                                               \
        if constexpr (SILIGEN_MIN_LOG_LEVEL <= 0) {                                    \
            auto logger = Siligen::Shared::Interfaces::ILoggingService::GetInstance(); \
            if (logger) logger->LogDebug(message, MODULE_NAME);                        \
        }                                                                              \
    } while (0)

#define SILIGEN_LOG_INFO(message)                                                      \
    do {                                                                               \
        if constexpr (SILIGEN_MIN_LOG_LEVEL <= 1) {                                    \
            auto logger = Siligen::Shared::Interfaces::ILoggingService::GetInstance(); \
            if (logger) logger->LogInfo(message, MODULE_NAME);                         \
        }                                                                              \
    } while (0)

#define SILIGEN_LOG_WARNING(message)                                                   \
    do {                                                                               \
        if constexpr (SILIGEN_MIN_LOG_LEVEL <= 2) {                                    \
            auto logger = Siligen::Shared::Interfaces::ILoggingService::GetInstance(); \
            if (logger) logger->LogWarning(message, MODULE_NAME);                      \
        }                                                                              \
    } while (0)

#define SILIGEN_LOG_ERROR(message)                                                     \
    do {                                                                               \
        if constexpr (SILIGEN_MIN_LOG_LEVEL <= 3) {                                    \
            auto logger = Siligen::Shared::Interfaces::ILoggingService::GetInstance(); \
            if (logger) logger->LogError(message, MODULE_NAME);                        \
        }                                                                              \
    } while (0)

#define SILIGEN_LOG_CRITICAL(message)                                              \
    do {                                                                           \
        auto logger = Siligen::Shared::Interfaces::ILoggingService::GetInstance(); \
        if (logger) logger->LogCritical(message, MODULE_NAME);                     \
    } while (0)

// 带上下文的日志宏 - 包含文件名和行号
#define SILIGEN_LOG_WITH_CONTEXT(level, message)                                             \
    do {                                                                                     \
        auto logger = Siligen::Shared::Interfaces::ILoggingService::GetInstance();           \
        if (logger) logger->LogWithContext(level, message, MODULE_NAME, __FILE__, __LINE__); \
    } while (0)

// ============================================================
// 格式化日志宏 - 避免字符串拼接开销（性能优化）
// ============================================================
// 使用 printf 风格的格式化字符串，避免运行时字符串拼接
// 示例：SILIGEN_LOG_ERROR_FMT("连接失败，错误码:%d", error_code);

// 注意：旧的 Logger 类已被移除，这些 _FMT 宏暂时禁用
// 请使用 shared/logging/PrintfLogFormatter.h 中的 SILIGEN_LOG_*_FMT_HELPER 宏
// 或者使用 SILIGEN_LOG_ERROR 等基础宏配合字符串格式化

// #define SILIGEN_LOG_DEBUG_FMT(format, ...)                                                                 \
//     do {                                                                                                   \
//         if constexpr (SILIGEN_MIN_LOG_LEVEL <= 0) {                                                        \
//             auto& logger = Siligen::Logger::GetInstance();                                                 \
//             logger.LogFormat(Siligen::Shared::Types::LogLevel::DEBUG, MODULE_NAME, format, ##__VA_ARGS__); \
//         }                                                                                                  \
//     } while (0)

// #define SILIGEN_LOG_INFO_FMT(format, ...)                                                                 \
//     do {                                                                                                  \
//         if constexpr (SILIGEN_MIN_LOG_LEVEL <= 1) {                                                       \
//             auto& logger = Siligen::Logger::GetInstance();                                                \
//             logger.LogFormat(Siligen::Shared::Types::LogLevel::INFO, MODULE_NAME, format, ##__VA_ARGS__); \
//         }                                                                                                 \
//     } while (0)

// #define SILIGEN_LOG_WARNING_FMT(format, ...)                                                                 \
//     do {                                                                                                     \
//         if constexpr (SILIGEN_MIN_LOG_LEVEL <= 2) {                                                          \
//             auto& logger = Siligen::Logger::GetInstance();                                                   \
//             logger.LogFormat(Siligen::Shared::Types::LogLevel::WARNING, MODULE_NAME, format, ##__VA_ARGS__); \
//         }                                                                                                    \
//     } while (0)

// #define SILIGEN_LOG_ERROR_FMT(format, ...)                                                               \
//     do {                                                                                                 \
//         if constexpr (SILIGEN_MIN_LOG_LEVEL <= 3) {                                                      \
//             auto& logger = Siligen::Logger::GetInstance();                                               \
//             logger.LogFormat(Siligen::Shared::Types::LogLevel::ERR, MODULE_NAME, format, ##__VA_ARGS__); \
//         }                                                                                                \
//     } while (0)

// #define SILIGEN_LOG_CRITICAL_FMT(format, ...)                                                             \
//     do {                                                                                                  \
//         auto& logger = Siligen::Logger::GetInstance();                                                    \
//         logger.LogFormat(Siligen::Shared::Types::LogLevel::CRITICAL, MODULE_NAME, format, ##__VA_ARGS__); \
//     } while (0)


