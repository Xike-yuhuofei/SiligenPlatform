#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <optional>

namespace Siligen::Adapters::Tcp {

/**
 * @brief JSON协议解析器
 *
 * 负责：
 * - 解析JSON请求
 * - 构建JSON响应
 * - 协议版本管理
 */
class JsonProtocol {
public:
    static constexpr const char* PROTOCOL_VERSION = "1.0";

    /// 解析JSON字符串，返回解析后的JSON对象
    static std::optional<nlohmann::json> Parse(const std::string& message);

    /// 构建成功响应
    static std::string MakeSuccessResponse(
        const std::string& id,
        const nlohmann::json& result);

    /// 构建错误响应
    static std::string MakeErrorResponse(
        const std::string& id,
        int code,
        const std::string& message);

    /// 构建事件推送消息
    static std::string MakeEventMessage(
        const std::string& event_type,
        const nlohmann::json& data);

    /// 构建pong响应
    static std::string MakePongResponse(const std::string& id);
};

} // namespace Siligen::Adapters::Tcp
