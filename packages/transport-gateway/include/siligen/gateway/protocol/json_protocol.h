#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace Siligen::Gateway::Protocol {

class JsonProtocol {
   public:
    static constexpr const char* kProtocolVersion = "1.0";

    static std::optional<nlohmann::json> Parse(const std::string& message);
    static std::string MakeSuccessResponse(const std::string& id, const nlohmann::json& result);
    static std::string MakeErrorResponse(const std::string& id, int code, const std::string& message);
    static std::string MakeEventMessage(const std::string& event_type, const nlohmann::json& data);
    static std::string MakePongResponse(const std::string& id);
};

}  // namespace Siligen::Gateway::Protocol
