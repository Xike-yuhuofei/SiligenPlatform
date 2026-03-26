#include "siligen/gateway/protocol/json_protocol.h"

#include <iostream>

namespace Siligen::Gateway::Protocol {

std::optional<nlohmann::json> JsonProtocol::Parse(const std::string& message) {
    try {
        return nlohmann::json::parse(message);
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "[JsonProtocol] Parse error: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::string JsonProtocol::MakeSuccessResponse(const std::string& id, const nlohmann::json& result) {
    nlohmann::json response = {
        {"id", id},
        {"version", kProtocolVersion},
        {"success", true},
        {"result", result},
    };
    return response.dump();
}

std::string JsonProtocol::MakeErrorResponse(const std::string& id, int code, const std::string& message) {
    nlohmann::json response = {
        {"id", id},
        {"version", kProtocolVersion},
        {"success", false},
        {"error", {{"code", code}, {"message", message}}},
    };
    return response.dump();
}

std::string JsonProtocol::MakeEventMessage(const std::string& event_type, const nlohmann::json& data) {
    nlohmann::json event = {
        {"version", kProtocolVersion},
        {"event", event_type},
        {"data", data},
    };
    return event.dump();
}

std::string JsonProtocol::MakePongResponse(const std::string& id) {
    return MakeSuccessResponse(id, {{"pong", true}});
}

}  // namespace Siligen::Gateway::Protocol
