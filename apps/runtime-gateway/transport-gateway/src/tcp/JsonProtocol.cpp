#include "JsonProtocol.h"

#include "siligen/gateway/protocol/json_protocol.h"

namespace Siligen::Adapters::Tcp {

std::optional<nlohmann::json> JsonProtocol::Parse(const std::string& message) {
    return Siligen::Gateway::Protocol::JsonProtocol::Parse(message);
}

std::string JsonProtocol::MakeSuccessResponse(
    const std::string& id,
    const nlohmann::json& result) {
    return Siligen::Gateway::Protocol::JsonProtocol::MakeSuccessResponse(id, result);
}

std::string JsonProtocol::MakeErrorResponse(
    const std::string& id,
    int code,
    const std::string& message) {
    return Siligen::Gateway::Protocol::JsonProtocol::MakeErrorResponse(id, code, message);
}

std::string JsonProtocol::MakeEventMessage(
    const std::string& event_type,
    const nlohmann::json& data) {
    return Siligen::Gateway::Protocol::JsonProtocol::MakeEventMessage(event_type, data);
}

std::string JsonProtocol::MakePongResponse(const std::string& id) {
    return Siligen::Gateway::Protocol::JsonProtocol::MakePongResponse(id);
}

} // namespace Siligen::Adapters::Tcp
