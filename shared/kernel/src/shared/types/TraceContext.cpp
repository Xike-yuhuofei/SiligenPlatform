// TraceContext.cpp - 追踪上下文实现
// Phase 1.1: 六边形架构日志系统最佳实践

#include "TraceContext.h"

#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>

namespace Siligen {
namespace Shared {
namespace Types {

// ============================================================
// thread_local 静态成员定义
// ============================================================

thread_local std::string TraceContext::trace_id_;
thread_local std::string TraceContext::span_id_;
thread_local std::unordered_map<std::string, std::string> TraceContext::attributes_;

// ============================================================
// 基础方法实现
// ============================================================

void TraceContext::SetTraceId(const std::string& trace_id) {
    trace_id_ = trace_id;
}

void TraceContext::SetSpanId(const std::string& span_id) {
    span_id_ = span_id;
}

void TraceContext::SetAttribute(const std::string& key, const std::string& value) {
    attributes_[key] = value;
}

void TraceContext::SetAttributes(const std::unordered_map<std::string, std::string>& attributes) {
    for (const auto& kv : attributes) {
        attributes_[kv.first] = kv.second;
    }
}

const std::string& TraceContext::GetTraceId() {
    return trace_id_;
}

const std::string& TraceContext::GetSpanId() {
    return span_id_;
}

std::string TraceContext::GetAttribute(const std::string& key, const std::string& default_value) {
    auto it = attributes_.find(key);
    if (it != attributes_.end()) {
        return it->second;
    }
    return default_value;
}

std::unordered_map<std::string, std::string> TraceContext::GetAllAttributes() {
    return attributes_;  // 返回副本
}

bool TraceContext::HasTraceContext() {
    return !trace_id_.empty();
}

void TraceContext::Clear() {
    trace_id_.clear();
    span_id_.clear();
    attributes_.clear();
}

std::string TraceContext::ToString() {
    std::ostringstream oss;
    bool first = true;

    if (!trace_id_.empty()) {
        oss << "trace_id=" << trace_id_;
        first = false;
    }

    if (!span_id_.empty()) {
        if (!first) oss << " ";
        oss << "span_id=" << span_id_;
        first = false;
    }

    // 添加关键属性 (选择性地)
    for (const auto& kv : attributes_) {
        // 只添加重要的属性，避免日志过长
        if (kv.first == "user_id" || kv.first == "request_id" || kv.first == "session_id") {
            if (!first) oss << " ";
            oss << kv.first << "=" << kv.second;
            first = false;
        }
    }

    return oss.str();
}

// ============================================================
// ID生成方法实现
// ============================================================

std::string TraceContext::GenerateTraceId() {
    return GenerateRandomHex(16);  // 16字节 = 32字符十六进制
}

std::string TraceContext::GenerateSpanId() {
    return GenerateRandomHex(8);  // 8字节 = 16字符十六进制
}

std::string TraceContext::GenerateRandomHex(size_t byte_count) {
    // 使用高精度随机数生成器
    static thread_local std::random_device rd;
    static thread_local std::mt19937_64 gen(rd());
    static thread_local std::uniform_int_distribution<uint64_t> dist;

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    size_t remaining = byte_count;
    while (remaining > 0) {
        // 每次生成8字节 (64位)
        size_t chunk = (remaining >= 8) ? 8 : remaining;
        uint64_t mask = (chunk == 8) ? 0xFFFFFFFFFFFFFFFFULL : ((1ULL << (chunk * 8)) - 1);
        uint64_t value = dist(gen) & mask;

        oss << std::setw(chunk * 2) << value;
        remaining -= chunk;
    }

    return oss.str();
}

}  // namespace Types
}  // namespace Shared
}  // namespace Siligen
