// TraceContext.h - 追踪上下文 (支持关联ID传播)
// Phase 1.1: 六边形架构日志系统最佳实践
//
// 设计原则:
// - 使用thread_local存储，每个线程独立的追踪上下文
// - 支持Trace ID和Span ID的传播
// - 支持自定义属性扩展
// - 遵循常见追踪ID/Span ID规范

#pragma once

#include <string>
#include <unordered_map>

namespace Siligen {
namespace Shared {
namespace Types {

/// @brief 追踪上下文 - 支持关联ID传播
/// @details 使用thread_local存储，每个线程独立的追踪上下文
/// 用于实现分布式追踪和日志关联
///
/// 使用示例:
/// @code
/// // 在请求入口处设置
/// TraceContext::SetTraceId("4bf92f3577b34da6a3ce929d0e0e4736");
/// TraceContext::SetSpanId("00f067aa0ba902b7");
///
/// // 在任何地方获取
/// std::string trace_id = TraceContext::GetTraceId();
///
/// // 添加自定义属性
/// TraceContext::SetAttribute("user_id", "12345");
/// TraceContext::SetAttribute("request_path", "/api/motion/start");
///
/// // 获取所有属性用于结构化日志
/// auto attrs = TraceContext::GetAllAttributes();
/// @endcode
///
/// 性能约束:
/// - thread_local访问开销极小 (无锁)
/// - 字符串使用移动语义避免拷贝
/// - GetTraceId()返回const引用避免拷贝
class TraceContext {
   public:
    /// @brief 设置追踪ID
    /// @param trace_id 追踪ID (通常为16字节十六进制字符串)
    /// @note 通常在请求入口处调用一次
    static void SetTraceId(const std::string& trace_id);

    /// @brief 设置跨度ID
    /// @param span_id 跨度ID (通常为8字节十六进制字符串)
    /// @note 可以在处理过程中更新以标识子操作
    static void SetSpanId(const std::string& span_id);

    /// @brief 添加自定义属性
    /// @param key 属性键名
    /// @param value 属性值
    /// @note 用于添加业务相关的追踪属性
    static void SetAttribute(const std::string& key, const std::string& value);

    /// @brief 批量设置属性
    /// @param attributes 属性映射表
    static void SetAttributes(const std::unordered_map<std::string, std::string>& attributes);

    /// @brief 获取追踪ID
    /// @return 追踪ID的const引用 (避免拷贝)
    /// @note 如果未设置，返回空字符串
    static const std::string& GetTraceId();

    /// @brief 获取跨度ID
    /// @return 跨度ID的const引用 (避免拷贝)
    /// @note 如果未设置，返回空字符串
    static const std::string& GetSpanId();

    /// @brief 获取指定属性值
    /// @param key 属性键名
    /// @param default_value 默认值 (如果属性不存在)
    /// @return 属性值
    static std::string GetAttribute(const std::string& key, const std::string& default_value = "");

    /// @brief 获取所有属性
    /// @return 属性映射表的const引用
    /// @note 用于结构化日志输出
    static const std::unordered_map<std::string, std::string>& GetAllAttributes();

    /// @brief 检查是否有追踪上下文
    /// @return true=有追踪ID, false=无追踪上下文
    static bool HasTraceContext();

    /// @brief 清空追踪上下文
    /// @note 通常在请求处理完成后调用，避免内存泄漏
    static void Clear();

    /// @brief 生成新的追踪ID
    /// @return 随机生成的16字节追踪ID (32字符十六进制)
    /// @note 用于启动新的追踪链
    static std::string GenerateTraceId();

    /// @brief 生成新的跨度ID
    /// @return 随机生成的8字节跨度ID (16字符十六进制)
    static std::string GenerateSpanId();

    // ============================================================
    // 便捷方法 - 用于日志格式化
    // ============================================================

    /// @brief 获取追踪信息的字符串表示
    /// @return 格式: "trace_id={trace_id} span_id={span_id}"
    static std::string ToString();

   private:
    // thread_local存储 - 每个线程独立的上下文
    static thread_local std::string trace_id_;
    static thread_local std::string span_id_;
    static thread_local std::unordered_map<std::string, std::string> attributes_;

    /// @brief 生成随机十六进制字符串
    /// @param byte_count 字节数
    /// @return 十六进制字符串 (长度 = byte_count * 2)
    static std::string GenerateRandomHex(size_t byte_count);
};

}  // namespace Types
}  // namespace Shared
}  // namespace Siligen
