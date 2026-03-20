// SpdlogLoggingAdapter.cpp - spdlog日志适配器实现
// Phase 1.3: 六边形架构日志系统最佳实践

#include "SpdlogLoggingAdapter.h"
#include "sinks/MemorySink.h"
#include "shared/types/TraceContext.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <chrono>
#include <sstream>

namespace Siligen {
namespace Infrastructure {
namespace Adapters {
namespace Logging {

// ============================================================
// 构造函数和析构函数
// ============================================================

SpdlogLoggingAdapter::SpdlogLoggingAdapter(const std::string& logger_name)
    : logger_name_(logger_name), is_async_(false) {

    // 默认配置
    current_config_.min_level = Shared::Types::LogLevel::INFO;
    current_config_.enable_console = true;
    current_config_.enable_file = false;
    current_config_.log_file_path = "logs/application.log";
    current_config_.enable_timestamp = true;
    current_config_.enable_thread_id = false;
    current_config_.max_file_size_mb = 10;
    current_config_.max_backup_files = 5;

    InitializeDefaultSinks();
}

SpdlogLoggingAdapter::~SpdlogLoggingAdapter() {
    if (logger_) {
        logger_->flush();
    }

    // 如果是异步模式，需要在程序退出前关闭
    // 注意: spdlog::shutdown() 在某些版本可能不可用，这里使用 drop()
    if (is_async_ && logger_) {
        logger_.reset();
    }
}

// ============================================================
// ILoggingService 接口实现
// ============================================================

Shared::Types::Result<void> SpdlogLoggingAdapter::Log(
    Shared::Types::LogLevel level,
    const std::string& message,
    const std::string& category) {

    if (!logger_) {
        return Shared::Types::Result<void>::Failure(
            Shared::Types::Error(
                Shared::Types::ErrorCode::LOG_INITIALIZATION_FAILED,
                "Logger not initialized",
                "SpdlogLoggingAdapter"
            )
        );
    }

    // 级别过滤
    if (!IsLevelEnabled(level)) {
        return Shared::Types::Result<void>::Success();
    }

    spdlog::level::level_enum spdlog_level = ConvertToSpdlogLevel(level);

    // 构建日志消息 (包含category和追踪信息)
    std::ostringstream oss;
    if (!category.empty() && category != "General") {
        oss << "[" << category << "] ";
    }
    oss << message;

    // 添加追踪上下文 (如果有)
    if (Shared::Types::TraceContext::HasTraceContext()) {
        std::string trace_info = Shared::Types::TraceContext::ToString();
        if (!trace_info.empty()) {
            oss << " (" << trace_info << ")";
        }
    }

    // 记录日志
    logger_->log(spdlog_level, oss.str());

    // 调用日志回调（用于测试）
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (log_callback_) {
            // 构造 LogEntry 用于回调
            Shared::Types::LogEntry entry;
            entry.level = level;
            entry.message = message;
            entry.category = category.empty() ? "General" : category;
            entry.timestamp = std::chrono::system_clock::now();

            log_callback_(entry);
        }
    }

    return Shared::Types::Result<void>::Success();
}

Shared::Types::Result<void> SpdlogLoggingAdapter::LogDebug(
    const std::string& message,
    const std::string& category) {

    return Log(Shared::Types::LogLevel::DEBUG, message, category);
}

Shared::Types::Result<void> SpdlogLoggingAdapter::LogInfo(
    const std::string& message,
    const std::string& category) {

    return Log(Shared::Types::LogLevel::INFO, message, category);
}

Shared::Types::Result<void> SpdlogLoggingAdapter::LogWarning(
    const std::string& message,
    const std::string& category) {

    return Log(Shared::Types::LogLevel::WARNING, message, category);
}

Shared::Types::Result<void> SpdlogLoggingAdapter::LogError(
    const std::string& message,
    const std::string& category) {

    return Log(Shared::Types::LogLevel::ERR, message, category);
}

Shared::Types::Result<void> SpdlogLoggingAdapter::LogCritical(
    const std::string& message,
    const std::string& category) {

    return Log(Shared::Types::LogLevel::CRITICAL, message, category);
}

Shared::Types::Result<void> SpdlogLoggingAdapter::SetLogLevel(Shared::Types::LogLevel level) {
    std::lock_guard<std::mutex> lock(config_mutex_);

    if (!logger_) {
        return Shared::Types::Result<void>::Failure(
            Shared::Types::Error(
                Shared::Types::ErrorCode::LOG_INITIALIZATION_FAILED,
                "Logger not initialized",
                "SpdlogLoggingAdapter"
            )
        );
    }

    spdlog::level::level_enum spdlog_level = ConvertToSpdlogLevel(level);

    // 更新 logger 级别
    logger_->set_level(spdlog_level);

    // 同步更新所有 sink 的级别，确保级别过滤生效
    for (auto& sink : logger_->sinks()) {
        sink->set_level(spdlog_level);
    }

    current_config_.min_level = level;

    return Shared::Types::Result<void>::Success();
}

Shared::Types::Result<Shared::Types::LogLevel> SpdlogLoggingAdapter::GetLogLevel() const {
    std::lock_guard<std::mutex> lock(config_mutex_);

    if (!logger_) {
        return Shared::Types::Result<Shared::Types::LogLevel>::Failure(
            Shared::Types::Error(
                Shared::Types::ErrorCode::LOG_INITIALIZATION_FAILED,
                "Logger not initialized",
                "SpdlogLoggingAdapter"
            )
        );
    }

    spdlog::level::level_enum spdlog_level = logger_->level();
    Shared::Types::LogLevel level = ConvertFromSpdlogLevel(spdlog_level);

    return Shared::Types::Result<Shared::Types::LogLevel>::Success(level);
}

Shared::Types::Result<void> SpdlogLoggingAdapter::Configure(
    const Shared::Types::LogConfiguration& config) {

    std::lock_guard<std::mutex> lock(config_mutex_);

    current_config_ = config;
    return RebuildLogger();
}

Shared::Types::Result<Shared::Types::LogConfiguration> SpdlogLoggingAdapter::GetConfiguration() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return Shared::Types::Result<Shared::Types::LogConfiguration>::Success(current_config_);
}

Shared::Types::Result<void> SpdlogLoggingAdapter::LogWithContext(
    Shared::Types::LogLevel level,
    const std::string& message,
    const std::string& category,
    const std::string& source_file,
    int32_t source_line) {

    if (!logger_) {
        return Shared::Types::Result<void>::Failure(
            Shared::Types::Error(
                Shared::Types::ErrorCode::LOG_INITIALIZATION_FAILED,
                "Logger not initialized",
                "SpdlogLoggingAdapter"
            )
        );
    }

    // 级别过滤
    if (!IsLevelEnabled(level)) {
        return Shared::Types::Result<void>::Success();
    }

    spdlog::level::level_enum spdlog_level = ConvertToSpdlogLevel(level);

    // 构建日志消息 (包含源文件位置)
    std::ostringstream oss;
    if (!category.empty() && category != "General") {
        oss << "[" << category << "] ";
    }
    oss << message;

    // 提取文件名 (不含路径)
    std::string filename = source_file;
    size_t pos = filename.find_last_of("/\\");
    if (pos != std::string::npos) {
        filename = filename.substr(pos + 1);
    }

    // 使用spdlog的源文件位置功能
    logger_->log(spdlog::source_loc{filename.c_str(), source_line, ""}, spdlog_level, oss.str());

    return Shared::Types::Result<void>::Success();
}

Shared::Types::Result<std::vector<Shared::Types::LogEntry>> SpdlogLoggingAdapter::GetRecentLogs(
    int32_t count,
    Shared::Types::LogLevel min_level,
    const std::string& category_filter) {

    if (!memory_sink_base_) {
        return Shared::Types::Result<std::vector<Shared::Types::LogEntry>>::Failure(
            Shared::Types::Error(
                Shared::Types::ErrorCode::LOG_INITIALIZATION_FAILED,
                "Memory sink not available",
                "SpdlogLoggingAdapter"
            )
        );
    }

    // 转换为MemorySink类型
    // 需要先转换为具体的 base_sink 类型
    using MemorySinkType = Siligen::Infrastructure::Logging::Sinks::MemorySink<std::mutex>;

    // 尝试动态转换 - MemorySink继承自base_sink
    auto* base_sink_ptr = dynamic_cast<spdlog::sinks::base_sink<std::mutex>*>(memory_sink_base_.get());
    if (!base_sink_ptr) {
        return Shared::Types::Result<std::vector<Shared::Types::LogEntry>>::Failure(
            Shared::Types::Error(
                Shared::Types::ErrorCode::LOG_INITIALIZATION_FAILED,
                "Memory sink is not a base_sink",
                "SpdlogLoggingAdapter"
            )
        );
    }

    auto* mem_sink = dynamic_cast<MemorySinkType*>(base_sink_ptr);
    if (!mem_sink) {
        return Shared::Types::Result<std::vector<Shared::Types::LogEntry>>::Failure(
            Shared::Types::Error(
                Shared::Types::ErrorCode::LOG_INITIALIZATION_FAILED,
                "Failed to cast to MemorySink",
                "SpdlogLoggingAdapter"
            )
        );
    }

    auto logs = mem_sink->GetRecent(count, min_level, category_filter);
    return Shared::Types::Result<std::vector<Shared::Types::LogEntry>>::Success(logs);
}

Shared::Types::Result<void> SpdlogLoggingAdapter::Flush() {
    if (!logger_) {
        return Shared::Types::Result<void>::Failure(
            Shared::Types::Error(
                Shared::Types::ErrorCode::LOG_INITIALIZATION_FAILED,
                "Logger not initialized",
                "SpdlogLoggingAdapter"
            )
        );
    }

    logger_->flush();
    return Shared::Types::Result<void>::Success();
}

bool SpdlogLoggingAdapter::IsLevelEnabled(Shared::Types::LogLevel level) const noexcept {
    if (!logger_) {
        return false;
    }

    spdlog::level::level_enum spdlog_level = ConvertToSpdlogLevel(level);
    return logger_->should_log(spdlog_level);
}

// ============================================================
// 测试支持接口实现
// ============================================================

void SpdlogLoggingAdapter::SetLogCallback(std::function<void(const Shared::Types::LogEntry&)> callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    log_callback_ = callback;
}

void SpdlogLoggingAdapter::ClearLogCallback() {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    log_callback_ = nullptr;
}

Shared::Types::Result<void> SpdlogLoggingAdapter::LogWithAttributes(
    Shared::Types::LogLevel level,
    const std::string& message,
    const std::string& category,
    const std::map<std::string, std::string>& attributes) {

    if (!logger_) {
        return Shared::Types::Result<void>::Failure(
            Shared::Types::Error(
                Shared::Types::ErrorCode::LOG_INITIALIZATION_FAILED,
                "Logger not initialized",
                "SpdlogLoggingAdapter"
            )
        );
    }

    // 级别过滤
    if (!IsLevelEnabled(level)) {
        return Shared::Types::Result<void>::Success();
    }

    spdlog::level::level_enum spdlog_level = ConvertToSpdlogLevel(level);

    // 构建带属性的消息
    std::ostringstream oss;
    if (!category.empty() && category != "General") {
        oss << "[" << category << "] ";
    }
    oss << message;

    if (!attributes.empty()) {
        oss << " {";
        bool first = true;
        for (const auto& [key, value] : attributes) {
            if (!first) {
                oss << ", ";
            }
            oss << key << "=" << value;
            first = false;
        }
        oss << "}";
    }

    // 记录日志
    logger_->log(spdlog_level, oss.str());

    // 调用日志回调（用于测试），带 metadata
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (log_callback_) {
            // 构造 LogEntry 用于回调，包含 metadata
            Shared::Types::LogEntry entry;
            entry.level = level;
            entry.message = message;
            entry.category = category.empty() ? "General" : category;
            entry.timestamp = std::chrono::system_clock::now();
            entry.metadata = attributes;  // 传递 metadata

            log_callback_(entry);
        }
    }

    return Shared::Types::Result<void>::Success();
}

// ============================================================
// spdlog特定配置方法
// ============================================================

Shared::Types::Result<void> SpdlogLoggingAdapter::EnableAsyncMode(size_t queue_size, bool overflow_behavior) {
    std::lock_guard<std::mutex> lock(config_mutex_);

    if (is_async_) {
        return Shared::Types::Result<void>::Success();  // 已经是异步模式
    }

    // 保存当前配置
    auto config = current_config_;

    // 重新构建为异步logger
    is_async_ = true;
    current_config_ = config;

    return RebuildLogger();
}

Shared::Types::Result<void> SpdlogLoggingAdapter::DisableAsyncMode() {
    std::lock_guard<std::mutex> lock(config_mutex_);

    if (!is_async_) {
        return Shared::Types::Result<void>::Success();  // 已经是同步模式
    }

    is_async_ = false;
    return RebuildLogger();
}

bool SpdlogLoggingAdapter::IsAsyncMode() const {
    return is_async_;
}

Shared::Types::Result<void> SpdlogLoggingAdapter::AddSink(std::shared_ptr<spdlog::sinks::sink> sink) {
    std::lock_guard<std::mutex> lock(config_mutex_);

    if (!logger_) {
        return Shared::Types::Result<void>::Failure(
            Shared::Types::Error(
                Shared::Types::ErrorCode::LOG_INITIALIZATION_FAILED,
                "Logger not initialized",
                "SpdlogLoggingAdapter"
            )
        );
    }

    logger_->sinks().push_back(sink);
    return Shared::Types::Result<void>::Success();
}

std::shared_ptr<spdlog::logger> SpdlogLoggingAdapter::GetUnderlyingLogger() {
    return logger_;
}

// ============================================================
// 私有方法实现
// ============================================================

spdlog::level::level_enum SpdlogLoggingAdapter::ConvertToSpdlogLevel(Shared::Types::LogLevel level) {
    switch (level) {
        case Shared::Types::LogLevel::DEBUG:    return spdlog::level::debug;
        case Shared::Types::LogLevel::INFO:     return spdlog::level::info;
        case Shared::Types::LogLevel::WARNING:  return spdlog::level::warn;
        case Shared::Types::LogLevel::ERR:      return spdlog::level::err;
        case Shared::Types::LogLevel::CRITICAL: return spdlog::level::critical;
        default:                                return spdlog::level::info;
    }
}

Shared::Types::LogLevel SpdlogLoggingAdapter::ConvertFromSpdlogLevel(spdlog::level::level_enum level) {
    switch (level) {
        case spdlog::level::debug:   return Shared::Types::LogLevel::DEBUG;
        case spdlog::level::info:    return Shared::Types::LogLevel::INFO;
        case spdlog::level::warn:    return Shared::Types::LogLevel::WARNING;
        case spdlog::level::err:     return Shared::Types::LogLevel::ERR;
        case spdlog::level::critical:return Shared::Types::LogLevel::CRITICAL;
        default:                      return Shared::Types::LogLevel::INFO;
    }
}

void SpdlogLoggingAdapter::InitializeDefaultSinks() {
    // 创建默认的同步logger，只包含控制台sink
    std::vector<spdlog::sink_ptr> sinks;

    auto console_sink = CreateConsoleSink();
    sinks.push_back(console_sink);

    // 创建内存sink (用于GetRecentLogs)
    memory_sink_base_ = CreateMemorySink();
    sinks.push_back(memory_sink_base_);

    // 创建logger
    logger_ = std::make_shared<spdlog::logger>(logger_name_, sinks.begin(), sinks.end());
    logger_->set_level(ConvertToSpdlogLevel(current_config_.min_level));
    logger_->flush_on(spdlog::level::info);

    // 注册到spdlog (如果 registry 可用)
    try {
        spdlog::register_logger(logger_);
        spdlog::set_default_logger(logger_);
    } catch (...) {
        // spdlog registry 可能不可用，忽略错误
    }
}

Shared::Types::Result<void> SpdlogLoggingAdapter::RebuildLogger() {
    // 收集sink
    std::vector<spdlog::sink_ptr> sinks;

    // 控制台sink
    if (current_config_.enable_console) {
        sinks.push_back(CreateConsoleSink());
    }

    // 文件sink
    if (current_config_.enable_file) {
        auto file_sink = CreateFileSink(current_config_.log_file_path);
        if (!file_sink) {
            return Shared::Types::Result<void>::Failure(
                Shared::Types::Error(
                    Shared::Types::ErrorCode::LOG_INITIALIZATION_FAILED,
                    "Failed to create file sink",
                    "SpdlogLoggingAdapter"
                )
            );
        }
        sinks.push_back(file_sink);
    }

    // 内存sink (始终保留)
    if (!memory_sink_base_) {
        memory_sink_base_ = CreateMemorySink();
    }
    sinks.push_back(memory_sink_base_);

    // 创建logger
    std::shared_ptr<spdlog::logger> new_logger;

    if (is_async_) {
        // 异步logger - 注意: 异步功能可能在某些spdlog版本不可用
        // 这里简化为同步logger
        new_logger = std::make_shared<spdlog::logger>(logger_name_, sinks.begin(), sinks.end());
    } else {
        // 同步logger
        new_logger = std::make_shared<spdlog::logger>(logger_name_, sinks.begin(), sinks.end());
    }

    new_logger->set_level(ConvertToSpdlogLevel(current_config_.min_level));
    new_logger->flush_on(spdlog::level::info);

    // 替换旧logger
    logger_ = new_logger;

    // 注册到spdlog (如果 registry 可用)
    try {
        spdlog::register_logger(logger_);
        spdlog::set_default_logger(logger_);
    } catch (...) {
        // spdlog registry 可能不可用，忽略错误
    }

    return Shared::Types::Result<void>::Success();
}

std::shared_ptr<spdlog::sinks::sink> SpdlogLoggingAdapter::CreateConsoleSink() {
    auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    // 设置格式: [时间] [级别] [分类] 消息
    sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

    // 设置 sink 级别与 logger 配置一致，确保级别过滤生效
    sink->set_level(ConvertToSpdlogLevel(current_config_.min_level));

    return sink;
}

std::shared_ptr<spdlog::sinks::sink> SpdlogLoggingAdapter::CreateFileSink(const std::string& file_path) {
    try {
        // 使用基本文件sink (未来可以替换为rotating_file_sink)
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(file_path, true);
        sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v");

        // 设置 sink 级别与 logger 配置一致，确保级别过滤生效
        sink->set_level(ConvertToSpdlogLevel(current_config_.min_level));

        return sink;
    } catch (const std::exception& e) {
        (void)e;  // 避免未使用变量警告
        return nullptr;
    }
}

std::shared_ptr<spdlog::sinks::sink> SpdlogLoggingAdapter::CreateMemorySink() {
    using MemorySinkType = Siligen::Infrastructure::Logging::Sinks::MemorySink_mt;
    return std::make_shared<MemorySinkType>(1000);
}

}  // namespace Logging
}  // namespace Adapters
}  // namespace Infrastructure
}  // namespace Siligen


