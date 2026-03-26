// SpdlogLoggingAdapter.h - spdlog日志适配器
// Phase 1.3: 六边形架构日志系统最佳实践
//
// 设计原则:
// - 实现ILoggingService接口 (六边形架构 - 适配器模式)
// - 使用spdlog作为底层日志库
// - 支持多目标输出 (控制台、文件、内存)
// - 支持同步和异步模式
// - 支持运行时配置

#pragma once

#include "shared/interfaces/ILoggingService.h"
#include "shared/types/LogTypes.h"
#include "shared/types/Result.h"
#include "spdlog/sinks/base_sink.h"
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace spdlog {
class logger;
namespace sinks {
class sink;
}
}  // namespace spdlog

namespace Siligen {
namespace Infrastructure {
namespace Adapters {
namespace Logging {

/// @brief spdlog日志适配器 - 实现ILoggingService接口
/// @details 使用spdlog作为底层日志库，支持异步日志和多目标输出
///
/// 功能特性:
/// - 实现ILoggingService所有接口
/// - 支持控制台、文件、内存缓冲三种输出目标
/// - 支持同步/异步模式切换
/// - 自动注入TraceContext (追踪ID/关联ID)
/// - 线程安全
///
/// 使用示例:
/// @code
/// // 创建适配器
/// auto adapter = std::make_shared<SpdlogLoggingAdapter>("siligen");
///
/// // 配置
/// Shared::Types::LogConfiguration config;
/// config.min_level = Shared::Types::LogLevel::INFO;
/// config.enable_console = true;
/// config.enable_file = true;
/// config.log_file_path = "logs/application.log";
/// adapter->Configure(config);
///
/// // 启用异步模式
/// adapter->EnableAsyncMode(8192);
///
/// // 记录日志
/// adapter->LogInfo("Application started", "System");
/// @endcode
class SpdlogLoggingAdapter : public Shared::Interfaces::ILoggingService {
public:
    /// @brief 构造函数
    /// @param logger_name 日志器名称 (用于区分不同模块的日志)
    explicit SpdlogLoggingAdapter(const std::string& logger_name = "siligen");

    /// @brief 析构函数
    virtual ~SpdlogLoggingAdapter();

    // 禁止拷贝和移动
    SpdlogLoggingAdapter(const SpdlogLoggingAdapter&) = delete;
    SpdlogLoggingAdapter& operator=(const SpdlogLoggingAdapter&) = delete;
    SpdlogLoggingAdapter(SpdlogLoggingAdapter&&) = delete;
    SpdlogLoggingAdapter& operator=(SpdlogLoggingAdapter&&) = delete;

    // ============================================================
    // ILoggingService 接口实现
    // ============================================================

    /// @brief 记录日志 (核心方法)
    Shared::Types::Result<void> Log(Shared::Types::LogLevel level,
                                     const std::string& message,
                                     const std::string& category = "General") override;

    /// @brief 记录调试日志
    Shared::Types::Result<void> LogDebug(const std::string& message,
                                          const std::string& category = "General") override;

    /// @brief 记录信息日志
    Shared::Types::Result<void> LogInfo(const std::string& message,
                                         const std::string& category = "General") override;

    /// @brief 记录警告日志
    Shared::Types::Result<void> LogWarning(const std::string& message,
                                            const std::string& category = "General") override;

    /// @brief 记录错误日志
    Shared::Types::Result<void> LogError(const std::string& message,
                                          const std::string& category = "General") override;

    /// @brief 记录严重错误日志
    Shared::Types::Result<void> LogCritical(const std::string& message,
                                             const std::string& category = "General") override;

    /// @brief 设置日志级别
    Shared::Types::Result<void> SetLogLevel(Shared::Types::LogLevel level) override;

    /// @brief 获取当前日志级别
    Shared::Types::Result<Shared::Types::LogLevel> GetLogLevel() const override;

    /// @brief 配置日志服务
    Shared::Types::Result<void> Configure(const Shared::Types::LogConfiguration& config) override;

    /// @brief 获取当前配置
    Shared::Types::Result<Shared::Types::LogConfiguration> GetConfiguration() const override;

    /// @brief 带上下文的日志记录 (包含文件名和行号)
    Shared::Types::Result<void> LogWithContext(Shared::Types::LogLevel level,
                                                const std::string& message,
                                                const std::string& category,
                                                const std::string& source_file,
                                                int32_t source_line) override;

    /// @brief 获取最近的日志条目
    Shared::Types::Result<std::vector<Shared::Types::LogEntry>> GetRecentLogs(
        int32_t count = 100,
        Shared::Types::LogLevel min_level = static_cast<Shared::Types::LogLevel>(0),
        const std::string& category_filter = "") override;

    /// @brief 刷新日志缓冲区
    Shared::Types::Result<void> Flush() override;

    // ============================================================
    // 测试支持接口实现
    // ============================================================

    /// @brief 设置日志回调（用于测试）
    void SetLogCallback(std::function<void(const Shared::Types::LogEntry&)> callback) override;

    /// @brief 清除日志回调
    void ClearLogCallback() override;

    /// @brief 带属性的日志记录（支持结构化数据）
    Shared::Types::Result<void> LogWithAttributes(
        Shared::Types::LogLevel level,
        const std::string& message,
        const std::string& category,
        const std::map<std::string, std::string>& attributes) override;

    /// @brief 获取实现类型名称
    const char* GetImplementationType() const noexcept override {
        return "SpdlogLoggingAdapter";
    }

    /// @brief 检查是否启用了指定日志级别
    bool IsLevelEnabled(Shared::Types::LogLevel level) const noexcept override;

    // ============================================================
    // spdlog特定配置方法
    // ============================================================

    /// @brief 设置异步日志模式
    /// @param queue_size 异步队列大小 (默认8192)
    /// @param overflow_behavior 溢出行为 (true=阻塞, false=丢弃)
    /// @note 启用后日志在后台线程处理，不阻塞业务逻辑
    Shared::Types::Result<void> EnableAsyncMode(size_t queue_size = 8192, bool overflow_behavior = true);

    /// @brief 切换回同步模式
    Shared::Types::Result<void> DisableAsyncMode();

    /// @brief 检查是否为异步模式
    bool IsAsyncMode() const;

    /// @brief 添加自定义Sink
    /// @param sink spdlog sink指针
    /// @note 用于添加自定义输出目标 (如网络日志、数据库等)
    Shared::Types::Result<void> AddSink(std::shared_ptr<spdlog::sinks::sink> sink);

    /// @brief 获取底层spdlog logger (用于高级用法)
    /// @warning 直接操作底层logger可能破坏封装，谨慎使用
    std::shared_ptr<spdlog::logger> GetUnderlyingLogger();

private:
    std::shared_ptr<spdlog::logger> logger_;
    std::shared_ptr<spdlog::sinks::sink> memory_sink_base_;  // 基类指针，用于添加到logger
    std::string logger_name_;
    mutable std::mutex config_mutex_;
    Shared::Types::LogConfiguration current_config_;
    bool is_async_;

    // 测试支持：日志回调
    std::function<void(const Shared::Types::LogEntry&)> log_callback_;
    mutable std::mutex callback_mutex_;

    // 指向具体的MemorySink的指针 (通过memory_sink_base_动态获取)
    // 注意: 不直接保存MemorySink指针以避免循环依赖

    /// @brief 转换LogLevel到spdlog level
    static spdlog::level::level_enum ConvertToSpdlogLevel(Shared::Types::LogLevel level);

    /// @brief 转换spdlog level到LogLevel
    static Shared::Types::LogLevel ConvertFromSpdlogLevel(spdlog::level::level_enum level);

    /// @brief 初始化默认Sink (控制台)
    void InitializeDefaultSinks();

    /// @brief 根据配置重新构建Logger
    Shared::Types::Result<void> RebuildLogger();

    /// @brief 创建控制台Sink
    std::shared_ptr<spdlog::sinks::sink> CreateConsoleSink();

    /// @brief 创建文件Sink
    std::shared_ptr<spdlog::sinks::sink> CreateFileSink(const std::string& file_path);

    /// @brief 创建内存Sink
    std::shared_ptr<spdlog::sinks::sink> CreateMemorySink();
};

}  // namespace Logging
}  // namespace Adapters
}  // namespace Infrastructure
}  // namespace Siligen
