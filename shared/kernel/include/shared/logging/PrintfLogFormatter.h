// PrintfLogFormatter.h - 轻量级日志格式化辅助工具
// 目的：为 Domain 层提供日志格式化功能，避免依赖基础设施层
#pragma once

#include "shared/types/Types.h"

#include <cstdarg>
#include <string>
#include <vector>

namespace Siligen {
namespace Shared {
namespace Util {

/// @brief 轻量级格式化辅助类
/// @details 提供 printf 风格的字符串格式化功能，避免 Domain 层依赖基础设施层的 Logger
class PrintfLogFormatter {
   public:
    /// @brief 格式化字符串（printf 风格）
    /// @param format 格式化字符串
    /// @param ... 可变参数
    /// @return 格式化后的字符串
    static std::string Format(const char* format, ...) {
        va_list args;
        va_start(args, format);
        int32 size = std::vsnprintf(nullptr, 0, format, args) + 1;
        va_end(args);

        if (size <= 0) return "";

        std::vector<char> buffer(size);
        va_start(args, format);
        std::vsnprintf(buffer.data(), size, format, args);
        va_end(args);

        return std::string(buffer.data(), size - 1);
    }
};

}  // namespace Util
}  // namespace Shared
}  // namespace Siligen

// 便捷宏定义 - 结合格式化辅助和日志宏（支持编译时级别过滤）
// 注意：这些宏会先格式化字符串，然后传递给日志宏
// 虽然仍有性能开销（格式化总是执行），但比字符串拼接更高效

// 编译时日志级别过滤（需要与 ILoggingService.h 保持一致）
#ifndef SILIGEN_MIN_LOG_LEVEL
#define SILIGEN_MIN_LOG_LEVEL 0  // 默认允许所有级别
#endif

#define SILIGEN_LOG_ERROR_FMT_HELPER(format, ...)                                                     \
    do {                                                                                              \
        if constexpr (SILIGEN_MIN_LOG_LEVEL <= 3) {                                                   \
            SILIGEN_LOG_ERROR(Siligen::Shared::Util::PrintfLogFormatter::Format(format, ##__VA_ARGS__)); \
        }                                                                                             \
    } while (0)

#define SILIGEN_LOG_WARNING_FMT_HELPER(format, ...)                                                     \
    do {                                                                                                \
        if constexpr (SILIGEN_MIN_LOG_LEVEL <= 2) {                                                     \
            SILIGEN_LOG_WARNING(Siligen::Shared::Util::PrintfLogFormatter::Format(format, ##__VA_ARGS__)); \
        }                                                                                               \
    } while (0)

#define SILIGEN_LOG_INFO_FMT_HELPER(format, ...)                                                     \
    do {                                                                                             \
        if constexpr (SILIGEN_MIN_LOG_LEVEL <= 1) {                                                  \
            SILIGEN_LOG_INFO(Siligen::Shared::Util::PrintfLogFormatter::Format(format, ##__VA_ARGS__)); \
        }                                                                                            \
    } while (0)

#define SILIGEN_LOG_DEBUG_FMT_HELPER(format, ...)                                                     \
    do {                                                                                              \
        if constexpr (SILIGEN_MIN_LOG_LEVEL <= 0) {                                                   \
            SILIGEN_LOG_DEBUG(Siligen::Shared::Util::PrintfLogFormatter::Format(format, ##__VA_ARGS__)); \
        }                                                                                             \
    } while (0)

