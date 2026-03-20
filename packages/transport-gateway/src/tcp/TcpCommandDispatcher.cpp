#include "TcpCommandDispatcher.h"
#include "siligen/gateway/protocol/json_protocol.h"
#include "shared/types/AxisTypes.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/types/Types.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "shared/types/TraceContext.h"

#include "facades/tcp/TcpDispensingFacade.h"
#include "facades/tcp/TcpMotionFacade.h"
#include "facades/tcp/TcpRecipeFacade.h"
#include "facades/tcp/TcpSystemFacade.h"
#include "domain/configuration/ports/IConfigurationPort.h"

#include "recipes/serialization/RecipeJsonSerializer.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>
#include <thread>

// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "TcpCommandDispatcher"

namespace {

namespace Application = Siligen::Application;
namespace TcpFacades = Siligen::Application::Facades::Tcp;

using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::float32;
using Siligen::Infrastructure::Adapters::Recipes::RecipeJsonSerializer;
using Siligen::Domain::Recipes::ValueObjects::ParameterValueEntry;
using Siligen::Domain::Recipes::ValueObjects::ImportConflict;
using Siligen::Domain::Recipes::ValueObjects::ConflictResolution;
using GatewayJsonProtocol = Siligen::Gateway::Protocol::JsonProtocol;

double ReadJsonDouble(const nlohmann::json& params, const char* key, double fallback) {
    if (!params.contains(key)) {
        return fallback;
    }
    try {
        return params.at(key).get<double>();
    } catch (...) {
        try {
            if (params.at(key).is_string()) {
                return std::stod(params.at(key).get<std::string>());
            }
        } catch (...) {
            return fallback;
        }
    }
    return fallback;
}

size_t ReadJsonSizeT(const nlohmann::json& params, const char* key, size_t fallback) {
    if (!params.contains(key)) {
        return fallback;
    }
    try {
        const auto value = params.at(key);
        if (value.is_number_unsigned()) {
            return value.get<size_t>();
        }
        if (value.is_number_integer()) {
            const auto parsed = value.get<int64_t>();
            return parsed < 0 ? fallback : static_cast<size_t>(parsed);
        }
        if (value.is_string()) {
            const auto text = value.get<std::string>();
            const auto parsed = std::stoll(text);
            return parsed < 0 ? fallback : static_cast<size_t>(parsed);
        }
    } catch (...) {
        return fallback;
    }
    return fallback;
}

void LogAxisSnapshot(
    const char* tag,
    const std::shared_ptr<TcpFacades::TcpMotionFacade>& motion_facade,
    Siligen::Shared::Types::LogicalAxisId axis_id) {
    if (!motion_facade) {
        return;
    }
    const char* axis_name = Siligen::Shared::Types::AxisName(axis_id);
    auto status_result = motion_facade->GetAxisMotionStatus(axis_id);
    if (status_result.IsSuccess()) {
        const auto& status = status_result.Value();
        SILIGEN_LOG_INFO_FMT_HELPER(
            "%s axis=%s state=%d pos=(%.3f,%.3f) vel=%.3f enabled=%d in_pos=%d alarm=%d soft+=%d soft-=%d hard+=%d hard-=%d follow=%d home=%d index=%d",
            tag,
            axis_name,
            static_cast<int>(status.state),
            status.position.x,
            status.position.y,
            status.velocity,
            status.enabled,
            status.in_position,
            status.servo_alarm,
            status.soft_limit_positive,
            status.soft_limit_negative,
            status.hard_limit_positive,
            status.hard_limit_negative,
            status.following_error,
            status.home_signal,
            status.index_signal);
    } else {
        SILIGEN_LOG_WARNING_FMT_HELPER("%s axis=%s status read failed: %s",
                                       tag,
                                       axis_name,
                                       status_result.GetError().GetMessage().c_str());
    }

}

std::string DescribeJsonField(const nlohmann::json& params, const char* key) {
    if (!params.contains(key)) {
        return "missing";
    }
    const auto& value = params.at(key);
    return std::string(value.type_name()) + ":" + value.dump();
}

void FlushLogs() {
    auto logger = Siligen::Shared::Interfaces::ILoggingService::GetInstance();
    if (logger) {
        logger->Flush();
    }
}

struct DxfBounds {
    double x_min = 0.0;
    double x_max = 0.0;
    double y_min = 0.0;
    double y_max = 0.0;
    bool has_value = false;
};

std::string ToUpperAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return value;
}

LogicalAxisId AxisFromName(std::string axis_name) {
    axis_name = ToUpperAscii(std::move(axis_name));
    if (axis_name == "X") {
        return LogicalAxisId::X;
    }
    if (axis_name == "Y") {
        return LogicalAxisId::Y;
    }
    if (axis_name == "Z") {
        return LogicalAxisId::Z;
    }
    if (axis_name == "U") {
        return LogicalAxisId::U;
    }
    return LogicalAxisId::INVALID;
}

std::string ExtractFilename(const std::string& path) {
    try {
        std::filesystem::path fs_path(path);
        return fs_path.filename().string();
    } catch (...) {
        return path;
    }
}

bool ReadFileToBuffer(const std::string& path, std::vector<uint8_t>& buffer, std::string& error_message) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        error_message = "File not found: " + path;
        return false;
    }

    std::ifstream::pos_type size = file.tellg();
    if (size <= 0) {
        error_message = "File is empty: " + path;
        return false;
    }

    buffer.resize(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    if (!file) {
        error_message = "Failed to read file: " + path;
        return false;
    }

    return true;
}

DxfBounds ComputeBounds(const std::vector<Siligen::TrajectoryPoint>& points) {
    DxfBounds bounds;
    if (points.empty()) {
        return bounds;
    }

    bounds.x_min = points.front().position.x;
    bounds.x_max = points.front().position.x;
    bounds.y_min = points.front().position.y;
    bounds.y_max = points.front().position.y;
    bounds.has_value = true;

    for (const auto& point : points) {
        bounds.x_min = std::min<double>(bounds.x_min, point.position.x);
        bounds.x_max = std::max<double>(bounds.x_max, point.position.x);
        bounds.y_min = std::min<double>(bounds.y_min, point.position.y);
        bounds.y_max = std::max<double>(bounds.y_max, point.position.y);
    }
    return bounds;
}

std::string TrimAscii(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(start, end - start);
}

std::string ToLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::vector<std::string> SplitCommaSeparated(const std::string& text) {
    std::vector<std::string> items;
    std::stringstream ss(text);
    std::string token;
    while (std::getline(ss, token, ',')) {
        auto trimmed = TrimAscii(token);
        if (!trimmed.empty()) {
            items.push_back(trimmed);
        }
    }
    return items;
}

std::vector<std::string> ParseTags(const nlohmann::json& params) {
    std::vector<std::string> tags;
    if (!params.contains("tags")) {
        return tags;
    }
    const auto& value = params.at("tags");
    if (value.is_array()) {
        for (const auto& entry : value) {
            if (entry.is_string()) {
                auto tag = TrimAscii(entry.get<std::string>());
                if (!tag.empty()) {
                    tags.push_back(tag);
                }
            }
        }
        return tags;
    }
    if (value.is_string()) {
        return SplitCommaSeparated(value.get<std::string>());
    }
    return tags;
}

bool ParseParameterEntries(const nlohmann::json& params,
                           std::vector<ParameterValueEntry>& out,
                           std::string& error) {
    out.clear();
    if (!params.contains("parameters")) {
        return true;
    }
    const auto& entries = params.at("parameters");
    if (!entries.is_array()) {
        error = "parameters must be an array";
        return false;
    }
    for (const auto& entry_json : entries) {
        if (!entry_json.is_object()) {
            error = "parameter entry must be an object";
            return false;
        }
        auto entry_result = RecipeJsonSerializer::ParameterValueEntryFromJson(entry_json);
        if (entry_result.IsError()) {
            error = entry_result.GetError().GetMessage();
            return false;
        }
        out.push_back(entry_result.Value());
    }
    return true;
}

nlohmann::json ImportConflictToJson(const ImportConflict& conflict) {
    return {
        {"type", RecipeJsonSerializer::ImportConflictTypeToString(conflict.type)},
        {"message", conflict.message},
        {"existingId", conflict.existing_id},
        {"incomingName", conflict.incoming_name},
        {"suggestedResolution", RecipeJsonSerializer::ConflictResolutionToString(conflict.suggested_resolution)}
    };
}

}  // namespace

namespace Siligen::Adapters::Tcp {

using Siligen::Shared::Types::FromIndex;
using Siligen::Shared::Types::IsValid;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Domain::Recipes::ValueObjects::ParameterValueEntry;
using Siligen::Infrastructure::Adapters::Recipes::RecipeJsonSerializer;

TcpCommandDispatcher::TcpCommandDispatcher(
    std::shared_ptr<Application::Facades::Tcp::TcpSystemFacade> systemFacade,
    std::shared_ptr<Application::Facades::Tcp::TcpMotionFacade> motionFacade,
    std::shared_ptr<Application::Facades::Tcp::TcpDispensingFacade> dispensingFacade,
    std::shared_ptr<Application::Facades::Tcp::TcpRecipeFacade> recipeFacade,
    std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> configPort,
    std::string config_path)
    : systemFacade_(std::move(systemFacade))
    , motionFacade_(std::move(motionFacade))
    , dispensingFacade_(std::move(dispensingFacade))
    , recipeFacade_(std::move(recipeFacade))
    , configPort_(std::move(configPort))
{
    config_path_ = std::move(config_path);
    if (config_path_.empty()) {
        config_path_ = "config/machine/machine_config.ini";
    }
    RegisterCommands();
    LoadDiagnosticsConfig();
}

void TcpCommandDispatcher::RegisterCommand(const std::string& name, CommandHandler handler) {
    commandHandlers_[name] = std::move(handler);
}

void TcpCommandDispatcher::RegisterCoreCommands() {
    RegisterCommand("ping", [this](const std::string& id, const nlohmann::json& params) { return HandlePing(id, params); });
    RegisterCommand("connect", [this](const std::string& id, const nlohmann::json& params) { return HandleConnect(id, params); });
    RegisterCommand("disconnect", [this](const std::string& id, const nlohmann::json& params) { return HandleDisconnect(id, params); });
    RegisterCommand("status", [this](const std::string& id, const nlohmann::json& params) { return HandleStatus(id, params); });
}

void TcpCommandDispatcher::RegisterMotionCommands() {
    RegisterCommand("home", [this](const std::string& id, const nlohmann::json& params) { return HandleHome(id, params); });
    RegisterCommand("home.go", [this](const std::string& id, const nlohmann::json& params) { return HandleHomeGo(id, params); });
    RegisterCommand("jog", [this](const std::string& id, const nlohmann::json& params) { return HandleJog(id, params); });
    RegisterCommand("move", [this](const std::string& id, const nlohmann::json& params) { return HandleMove(id, params); });
    RegisterCommand("stop", [this](const std::string& id, const nlohmann::json& params) { return HandleStop(id, params); });
    RegisterCommand("estop", [this](const std::string& id, const nlohmann::json& params) { return HandleEstop(id, params); });
}

void TcpCommandDispatcher::RegisterDispensingCommands() {
    RegisterCommand("dispenser.start", [this](const std::string& id, const nlohmann::json& params) { return HandleDispenserStart(id, params); });
    RegisterCommand("dispenser.stop", [this](const std::string& id, const nlohmann::json& params) { return HandleDispenserStop(id, params); });
    RegisterCommand("dispenser.pause", [this](const std::string& id, const nlohmann::json& params) { return HandleDispenserPause(id, params); });
    RegisterCommand("dispenser.resume", [this](const std::string& id, const nlohmann::json& params) { return HandleDispenserResume(id, params); });
    RegisterCommand("purge", [this](const std::string& id, const nlohmann::json& params) { return HandlePurge(id, params); });
    RegisterCommand("supply.open", [this](const std::string& id, const nlohmann::json& params) { return HandleSupplyOpen(id, params); });
    RegisterCommand("supply.close", [this](const std::string& id, const nlohmann::json& params) { return HandleSupplyClose(id, params); });
}

void TcpCommandDispatcher::RegisterDxfCommands() {
    RegisterCommand("dxf.load", [this](const std::string& id, const nlohmann::json& params) { return HandleDxfLoad(id, params); });
    RegisterCommand("dxf.execute", [this](const std::string& id, const nlohmann::json& params) { return HandleDxfExecute(id, params); });
    RegisterCommand("dxf.pause", [this](const std::string& id, const nlohmann::json& params) { return HandleDxfPause(id, params); });
    RegisterCommand("dxf.resume", [this](const std::string& id, const nlohmann::json& params) { return HandleDxfResume(id, params); });
    RegisterCommand("dxf.stop", [this](const std::string& id, const nlohmann::json& params) { return HandleDxfStop(id, params); });
    RegisterCommand("dxf.info", [this](const std::string& id, const nlohmann::json& params) { return HandleDxfInfo(id, params); });
    RegisterCommand("dxf.progress", [this](const std::string& id, const nlohmann::json& params) { return HandleDxfProgress(id, params); });
}

void TcpCommandDispatcher::RegisterAlarmCommands() {
    RegisterCommand("alarms.list", [this](const std::string& id, const nlohmann::json& params) { return HandleAlarmsList(id, params); });
    RegisterCommand("alarms.clear", [this](const std::string& id, const nlohmann::json& params) { return HandleAlarmsClear(id, params); });
    RegisterCommand("alarms.acknowledge", [this](const std::string& id, const nlohmann::json& params) { return HandleAlarmsAcknowledge(id, params); });
}

void TcpCommandDispatcher::RegisterRecipeCommands() {
    RegisterCommand("recipe.list", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeList(id, params); });
    RegisterCommand("recipe.get", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeGet(id, params); });
    RegisterCommand("recipe.create", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeCreate(id, params); });
    RegisterCommand("recipe.update", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeUpdate(id, params); });
    RegisterCommand("recipe.archive", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeArchive(id, params); });
    RegisterCommand("recipe.draft.create", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeDraftCreate(id, params); });
    RegisterCommand("recipe.draft.update", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeDraftUpdate(id, params); });
    RegisterCommand("recipe.publish", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipePublish(id, params); });
    RegisterCommand("recipe.versions", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeVersions(id, params); });
    RegisterCommand("recipe.version.create", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeVersionCreate(id, params); });
    RegisterCommand("recipe.version.compare", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeCompare(id, params); });
    RegisterCommand("recipe.version.activate", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeActivate(id, params); });
    RegisterCommand("recipe.templates", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeTemplates(id, params); });
    RegisterCommand("recipe.schema.default", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeSchemaDefault(id, params); });
    RegisterCommand("recipe.audit", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeAudit(id, params); });
    RegisterCommand("recipe.export", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeExport(id, params); });
    RegisterCommand("recipe.import", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeImport(id, params); });
}

void TcpCommandDispatcher::RegisterCommands() {
    RegisterCoreCommands();
    RegisterMotionCommands();
    RegisterDispensingCommands();
    RegisterDxfCommands();
    RegisterAlarmCommands();
    RegisterRecipeCommands();
}

void TcpCommandDispatcher::LoadDiagnosticsConfig() {
    if (!configPort_) {
        return;
    }
    auto result = configPort_->GetDiagnosticsConfig();
    if (result.IsSuccess()) {
        diagnostics_config_ = result.Value();
        SILIGEN_LOG_INFO("TcpCommandDispatcher diagnostics: " + diagnostics_config_.ToString());
    } else {
        SILIGEN_LOG_WARNING("TcpCommandDispatcher diagnostics load failed: " + result.GetError().GetMessage());
    }
}

bool TcpCommandDispatcher::IsDeepTcpLoggingEnabled() const {
    return diagnostics_config_.deep_tcp_logging;
}

bool TcpCommandDispatcher::IsDeepMotionLoggingEnabled() const {
    return diagnostics_config_.deep_motion_logging;
}

std::string TcpCommandDispatcher::TruncatePayload(const std::string& payload) const {
    const size_t limit = diagnostics_config_.tcp_payload_max_chars > 0
        ? static_cast<size_t>(diagnostics_config_.tcp_payload_max_chars)
        : payload.size();
    if (payload.size() <= limit) {
        return payload;
    }
    return payload.substr(0, limit) + "...";
}

std::string TcpCommandDispatcher::Dispatch(const std::string& message) {
    if (IsDeepTcpLoggingEnabled()) {
        SILIGEN_LOG_INFO("TCP RX len=" + std::to_string(message.size()) +
                         " payload=" + TruncatePayload(message));
    }
    auto jsonOpt = GatewayJsonProtocol::Parse(message);
    if (!jsonOpt) {
        if (IsDeepTcpLoggingEnabled()) {
            SILIGEN_LOG_WARNING("TCP RX parse failed: invalid JSON");
        }
        return GatewayJsonProtocol::MakeErrorResponse("", 1000, "Invalid JSON format");
    }

    const auto& json = *jsonOpt;

    // 提取请求字段
    std::string id = json.value("id", "");
    std::string method = json.value("method", "");
    nlohmann::json params = json.value("params", nlohmann::json::object());

    if (method.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 1001, "Missing 'method' field");
    }

    std::string trace_id = id.empty()
        ? Siligen::Shared::Types::TraceContext::GenerateTraceId()
        : id;
    Siligen::Shared::Types::TraceContext::SetTraceId(trace_id);
    if (!id.empty()) {
        Siligen::Shared::Types::TraceContext::SetAttribute("request_id", id);
    }

    if (method == "dxf.execute") {
        SILIGEN_LOG_INFO("DXF原始请求: " + message);
        SILIGEN_LOG_INFO("DXF解析params: " + params.dump());
    }

    if (IsDeepTcpLoggingEnabled()) {
        SILIGEN_LOG_INFO("TCP Dispatch method=" + method +
                         " id=" + id +
                         " params=" + TruncatePayload(params.dump()));
    }

    // 查找并执行命令处理器
    auto it = commandHandlers_.find(method);
    if (it == commandHandlers_.end()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 1002, "Unknown method: " + method);
    }

    try {
        auto response = it->second(id, params);
        if (IsDeepTcpLoggingEnabled()) {
            SILIGEN_LOG_INFO("TCP TX len=" + std::to_string(response.size()) +
                             " response=" + TruncatePayload(response));
        }
        return response;
    } catch (const std::exception& e) {
        if (IsDeepTcpLoggingEnabled()) {
            SILIGEN_LOG_WARNING(std::string("TCP handler exception: ") + e.what());
        }
        return GatewayJsonProtocol::MakeErrorResponse(id, 1003, std::string("Handler error: ") + e.what());
    }
}

// ========== 命令处理实现 ==========

std::string TcpCommandDispatcher::HandlePing(const std::string& id, const nlohmann::json& /*params*/) {
    return GatewayJsonProtocol::MakePongResponse(id);
}

std::string TcpCommandDispatcher::HandleConnect(const std::string& id, const nlohmann::json& params) {
    if (!systemFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2000, "TcpSystemFacade not available");
    }

    Application::UseCases::System::InitializeSystemRequest request;
    request.auto_connect_hardware = true;

    // 从params提取连接配置
    if (params.contains("card_ip")) {
        request.connection_config.card_ip = params["card_ip"].get<std::string>();
    }
    if (params.contains("local_ip")) {
        request.connection_config.local_ip = params["local_ip"].get<std::string>();
    }

    auto result = systemFacade_->Initialize(request);
    if (!result.IsSuccess()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2001, result.GetError().GetMessage());
    }

    auto response = result.Value();
    nlohmann::json resultJson = {
        {"connected", response.hardware_connected},
        {"message", response.status_message}
    };
    return GatewayJsonProtocol::MakeSuccessResponse(id, resultJson);
}

std::string TcpCommandDispatcher::HandleDisconnect(const std::string& id, const nlohmann::json& /*params*/) {
    // 断开连接逻辑（简化实现）
    nlohmann::json resultJson = {{"disconnected", true}};
    return GatewayJsonProtocol::MakeSuccessResponse(id, resultJson);
}

std::string TcpCommandDispatcher::HandleStatus(const std::string& id, const nlohmann::json& /*params*/) {
    if (!motionFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2100, "TcpMotionFacade not available");
    }

    nlohmann::json axesJson = nlohmann::json::object();

    // 获取各轴状态
    auto allStatusResult = motionFacade_->GetAllAxesMotionStatus();
    if (allStatusResult.IsSuccess()) {
        const auto& statuses = allStatusResult.Value();
        const char* axisNames[] = {"X", "Y", "Z", "U"};
        for (size_t i = 0; i < statuses.size() && i < 4; ++i) {
            const auto& status = statuses[i];
            bool isHomed = (status.state == Domain::Motion::Ports::MotionState::HOMED);
            nlohmann::json axisJson = {
                {"position", status.position.x},
                {"velocity", status.velocity},
                {"enabled", status.enabled},
                {"homed", isHomed}
            };
            axesJson[axisNames[i]] = axisJson;
        }
    }

    // 获取当前位置
    auto posResult = motionFacade_->GetCurrentPosition();
    nlohmann::json positionJson = nlohmann::json::object();
    if (posResult.IsSuccess()) {
        auto pos = posResult.Value();
        positionJson = {{"x", pos.x}, {"y", pos.y}};
    }

    // 获取点胶阀状态
    nlohmann::json dispenserJson = {{"valve_open", false}, {"supply_open", false}};
    if (dispensingFacade_) {
        auto statusResult = dispensingFacade_->GetDispenserStatus();
        if (statusResult.IsSuccess()) {
            auto status = statusResult.Value();
            dispenserJson["valve_open"] = status.IsRunning();
        }

        auto supplyResult = dispensingFacade_->GetSupplyStatus();
        if (supplyResult.IsSuccess()) {
            auto supply = supplyResult.Value();
            dispenserJson["supply_open"] = supply.IsOpen();
        }
    }

    nlohmann::json ioJson = {
        {"limit_x_pos", false},
        {"limit_x_neg", false},
        {"limit_y_pos", false},
        {"limit_y_neg", false},
        {"estop", false},
        {"door", false}
    };
    auto limitXPos = motionFacade_->ReadLimitStatus(LogicalAxisId::X, true);
    if (limitXPos.IsSuccess()) {
        ioJson["limit_x_pos"] = limitXPos.Value();
    }
    auto limitXNeg = motionFacade_->ReadLimitStatus(LogicalAxisId::X, false);
    if (limitXNeg.IsSuccess()) {
        ioJson["limit_x_neg"] = limitXNeg.Value();
    }
    auto limitYPos = motionFacade_->ReadLimitStatus(LogicalAxisId::Y, true);
    if (limitYPos.IsSuccess()) {
        ioJson["limit_y_pos"] = limitYPos.Value();
    }
    auto limitYNeg = motionFacade_->ReadLimitStatus(LogicalAxisId::Y, false);
    if (limitYNeg.IsSuccess()) {
        ioJson["limit_y_neg"] = limitYNeg.Value();
    }

    nlohmann::json resultJson = {
        {"connected", true},
        {"machine_state", "Ready"},
        {"axes", axesJson},
        {"position", positionJson},
        {"io", ioJson},
        {"dispenser", dispenserJson},
        {"alarms", nlohmann::json::array()}
    };

    return GatewayJsonProtocol::MakeSuccessResponse(id, resultJson);
}

std::string TcpCommandDispatcher::HandleHome(const std::string& id, const nlohmann::json& params) {
    if (!motionFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2200, "TcpMotionFacade not available");
    }

    Application::UseCases::Motion::Homing::HomeAxesRequest request;

    if (params.contains("axes") && params["axes"].is_array()) {
        for (const auto& axis : params["axes"]) {
            if (!axis.is_string()) {
                return GatewayJsonProtocol::MakeErrorResponse(id, 2202, "Invalid axis name");
            }
            auto axis_id = AxisFromName(axis.get<std::string>());
            if (!IsValid(axis_id)) {
                return GatewayJsonProtocol::MakeErrorResponse(id, 2202, "Invalid axis name");
            }
            request.axes.push_back(axis_id);
        }
        if (request.axes.empty()) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2202, "Empty axis list");
        }
    } else {
        request.home_all_axes = true;
    }

    if (params.contains("force")) {
        if (!params["force"].is_boolean()) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2202, "Invalid force flag");
        }
        request.force_rehome = params["force"].get<bool>();
    }

    auto result = motionFacade_->Home(request);
    if (!result.IsSuccess()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2201, result.GetError().GetMessage());
    }

    auto response = result.Value();
    nlohmann::json resultJson = {
        {"completed", response.all_completed},
        {"message", response.status_message}
    };
    return GatewayJsonProtocol::MakeSuccessResponse(id, resultJson);
}

std::string TcpCommandDispatcher::HandleHomeGo(const std::string& id, const nlohmann::json& params) {
    if (!motionFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2410, "TcpMotionFacade not available");
    }

    std::vector<LogicalAxisId> axes;
    if (params.contains("axes") && params["axes"].is_array()) {
        for (const auto& axis : params["axes"]) {
            if (!axis.is_string()) {
                return GatewayJsonProtocol::MakeErrorResponse(id, 2412, "Invalid axis name");
            }
            auto axis_id = AxisFromName(axis.get<std::string>());
            if (!IsValid(axis_id)) {
                return GatewayJsonProtocol::MakeErrorResponse(id, 2412, "Invalid axis name");
            }
            axes.push_back(axis_id);
        }
        if (axes.empty()) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2412, "Empty axis list");
        }
    } else {
        int axis_count = 2;
        if (configPort_) {
            auto config_result = configPort_->GetHardwareConfiguration();
            if (config_result.IsSuccess()) {
                axis_count = static_cast<int>(config_result.Value().EffectiveAxisCount());
            }
        }
        for (int axis = 0; axis < axis_count; ++axis) {
            auto axis_id = FromIndex(static_cast<int16>(axis));
            if (IsValid(axis_id)) {
                axes.push_back(axis_id);
            }
        }
    }

    for (auto axis_id : axes) {
        auto homed_result = motionFacade_->IsAxisHomed(axis_id);
        if (homed_result.IsError()) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2413, homed_result.GetError().GetMessage());
        }
        if (!homed_result.Value()) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2414, "Axis not homed, run homing first");
        }
    }

    std::optional<float> speed_override;
    if (params.contains("speed")) {
        speed_override = params.value("speed", 0.0f);
    }
    float speed = 0.0f;
    if (speed_override.has_value()) {
        speed = speed_override.value();
    } else if (configPort_) {
        auto machine_result = configPort_->GetMachineConfig();
        if (machine_result.IsSuccess()) {
            speed = machine_result.Value().max_speed;
        }
    }
    if (speed <= 0.0f) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2415, "Invalid move speed");
    }

    for (auto axis_id : axes) {
        Application::UseCases::Motion::Manual::ManualMotionCommand cmd;
        cmd.axis = axis_id;
        cmd.position = 0.0f;
        cmd.velocity = speed;
        auto result = motionFacade_->ExecutePointToPointMotion(cmd, false);
        if (!result.IsSuccess()) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2416, result.GetError().GetMessage());
        }
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"moving", true}, {"message", "Go home started"}});
}

std::string TcpCommandDispatcher::HandleJog(const std::string& id, const nlohmann::json& params) {
    if (!motionFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2300, "TcpMotionFacade not available");
    }

    // 解析参数
    std::string axisName = params.value("axis", "X");
    int direction = params.value("direction", 1);
    std::optional<float> speed_override;
    if (params.contains("speed")) {
        speed_override = params.value("speed", 0.0f);
    }

    auto axis_id = AxisFromName(axisName);
    if (!IsValid(axis_id)) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2302, "Invalid axis name");
    }

    float speed = 0.0f;
    if (speed_override.has_value()) {
        speed = speed_override.value();
    } else {
        if (!configPort_) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2303, "Config port not available");
        }
        auto machine_result = configPort_->GetMachineConfig();
        if (machine_result.IsError()) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2303, machine_result.GetError().GetMessage());
        }
        speed = machine_result.Value().max_speed;
    }
    if (IsDeepMotionLoggingEnabled()) {
        SILIGEN_LOG_INFO("Jog deep params: axis=" + axisName +
                         " dir=" + std::to_string(direction) +
                         " speed=" + std::to_string(speed) +
                         " params=" + TruncatePayload(params.dump()));
        LogAxisSnapshot("Jog deep pre", motionFacade_, axis_id);
    }

    SILIGEN_LOG_INFO_FMT_HELPER("Jog request: axis=%s dir=%d speed=%.3f",
                                axisName.c_str(),
                                direction,
                                speed);
    LogAxisSnapshot("Jog pre", motionFacade_, axis_id);

    auto result = motionFacade_->StartJog(axis_id, static_cast<int16>(direction), speed);
    if (!result.IsSuccess()) {
        const auto& error = result.GetError();
        if (error.GetCode() == Siligen::Shared::Types::ErrorCode::VELOCITY_LIMIT_EXCEEDED) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2303, "Invalid jog speed");
        }
        return GatewayJsonProtocol::MakeErrorResponse(id, 2301, error.GetMessage());
    }

    LogAxisSnapshot("Jog post", motionFacade_, axis_id);
    if (IsDeepMotionLoggingEnabled() && diagnostics_config_.snapshot_delay_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(diagnostics_config_.snapshot_delay_ms));
        LogAxisSnapshot("Jog deep delayed", motionFacade_, axis_id);
    }
    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"jogging", true}});
}

std::string TcpCommandDispatcher::HandleMove(const std::string& id, const nlohmann::json& params) {
    if (!motionFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2400, "TcpMotionFacade not available");
    }

    // 解析目标位置
    float x = params.value("x", 0.0f);
    float y = params.value("y", 0.0f);
    std::optional<float> speed_override;
    if (params.contains("speed")) {
        speed_override = params.value("speed", 0.0f);
    }

    float speed = 0.0f;
    if (speed_override.has_value()) {
        speed = speed_override.value();
    } else {
        if (!configPort_) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2402, "Config port not available");
        }
        auto machine_result = configPort_->GetMachineConfig();
        if (machine_result.IsError()) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2402, machine_result.GetError().GetMessage());
        }
        speed = machine_result.Value().max_speed;
    }
    if (speed <= 0.0f) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2402, "Invalid move speed");
    }

    if (IsDeepMotionLoggingEnabled()) {
        SILIGEN_LOG_INFO("Move deep params: x=" + std::to_string(x) +
                         " y=" + std::to_string(y) +
                         " speed=" + std::to_string(speed) +
                         " params=" + TruncatePayload(params.dump()));
        LogAxisSnapshot("Move deep pre", motionFacade_, LogicalAxisId::X);
        LogAxisSnapshot("Move deep pre", motionFacade_, LogicalAxisId::Y);
    }

    // 执行X轴移动
    Application::UseCases::Motion::Manual::ManualMotionCommand cmdX;
    cmdX.axis = LogicalAxisId::X;
    cmdX.position = x;
    cmdX.velocity = speed;
    auto resultX = motionFacade_->ExecutePointToPointMotion(cmdX);

    // 执行Y轴移动
    Application::UseCases::Motion::Manual::ManualMotionCommand cmdY;
    cmdY.axis = LogicalAxisId::Y;
    cmdY.position = y;
    cmdY.velocity = speed;
    auto resultY = motionFacade_->ExecutePointToPointMotion(cmdY);

    if (!resultX.IsSuccess() || !resultY.IsSuccess()) {
        std::string errMsg = resultX.IsSuccess() ? resultY.GetError().GetMessage() : resultX.GetError().GetMessage();
        return GatewayJsonProtocol::MakeErrorResponse(id, 2401, errMsg);
    }

    if (IsDeepMotionLoggingEnabled()) {
        LogAxisSnapshot("Move deep post", motionFacade_, LogicalAxisId::X);
        LogAxisSnapshot("Move deep post", motionFacade_, LogicalAxisId::Y);
        if (diagnostics_config_.snapshot_delay_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(diagnostics_config_.snapshot_delay_ms));
            LogAxisSnapshot("Move deep delayed", motionFacade_, LogicalAxisId::X);
            LogAxisSnapshot("Move deep delayed", motionFacade_, LogicalAxisId::Y);
        }
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"moving", true}});
}

std::string TcpCommandDispatcher::HandleStop(const std::string& id, const nlohmann::json& /*params*/) {
    if (!motionFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2500, "TcpMotionFacade not available");
    }

    SILIGEN_LOG_INFO("Stop request received");
    if (IsDeepMotionLoggingEnabled()) {
        SILIGEN_LOG_INFO("Stop deep request received");
    }

    // 停止所有轴的JOG运动
    for (int16_t axis_index = 0; axis_index < 4; ++axis_index) {
        auto axis_id = FromIndex(axis_index);
        if (!IsValid(axis_id)) {
            continue;
        }
        LogAxisSnapshot("Stop pre", motionFacade_, axis_id);
        auto stop_result = motionFacade_->StopJog(axis_id);
        if (!stop_result.IsSuccess()) {
            SILIGEN_LOG_WARNING_FMT_HELPER("Stop failed axis=%s reason=%s",
                                           Siligen::Shared::Types::AxisName(axis_id),
                                           stop_result.GetError().GetMessage().c_str());
        }
        LogAxisSnapshot("Stop post", motionFacade_, axis_id);
        if (IsDeepMotionLoggingEnabled() && diagnostics_config_.snapshot_after_stop_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(diagnostics_config_.snapshot_after_stop_ms));
            LogAxisSnapshot("Stop deep delayed", motionFacade_, axis_id);
        }
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"stopped", true}});
}

std::string TcpCommandDispatcher::HandleEstop(const std::string& id, const nlohmann::json& /*params*/) {
    if (!systemFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2600, "TcpSystemFacade not available");
    }

    Application::UseCases::System::EmergencyStopRequest request;
    request.reason = Application::UseCases::System::EmergencyStopReason::USER_REQUEST;
    request.detail_message = "HMI emergency stop request";

    auto result = systemFacade_->EmergencyStop(request);
    if (!result.IsSuccess()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2601, result.GetError().GetMessage());
    }

    auto response = result.Value();
    nlohmann::json resultJson = {
        {"motion_stopped", response.motion_stopped},
        {"message", response.status_message}
    };
    return GatewayJsonProtocol::MakeSuccessResponse(id, resultJson);
}

std::string TcpCommandDispatcher::HandleDispenserStart(const std::string& id, const nlohmann::json& params) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2700, "TcpDispensingFacade not available");
    }

    // 解析点胶参数
    Domain::Dispensing::Ports::DispenserValveParams valveParams;
    valveParams.count = params.value("count", 1u);
    valveParams.intervalMs = params.value("interval_ms", 1000u);
    valveParams.durationMs = params.value("duration_ms", 100u);

    // 验证参数
    if (!valveParams.IsValid()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2702, valveParams.GetValidationError());
    }

    auto result = dispensingFacade_->StartDispenser(valveParams);
    if (!result.IsSuccess()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2701, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"dispensing", true}});
}

std::string TcpCommandDispatcher::HandleDispenserStop(const std::string& id, const nlohmann::json& /*params*/) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2800, "TcpDispensingFacade not available");
    }

    auto result = dispensingFacade_->StopDispenser();
    if (!result.IsSuccess()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2801, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"dispensing", false}});
}

std::string TcpCommandDispatcher::HandleDispenserPause(const std::string& id, const nlohmann::json& /*params*/) {
    // 安全检查：如果有DXF任务正在运行，强制停止它（因为目前DXF不支持暂停）
    std::string currentDxfTask;
    {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        currentDxfTask = dxf_task_id_;
    }

    if (!currentDxfTask.empty()) {
        SILIGEN_LOG_WARNING("收到暂停请求，但检测到DXF任务正在运行且不支持暂停。执行强制停止以确保安全。");
        if (dispensingFacade_) {
            dispensingFacade_->CancelDxfTask(currentDxfTask);
            dispensingFacade_->StopDxfExecution();
            
            // 清除任务ID，避免重复停止
            {
                std::lock_guard<std::mutex> lock(dxf_mutex_);
                dxf_task_id_.clear();
            }
        }
    }

    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2810, "TcpDispensingFacade not available");
    }

    auto result = dispensingFacade_->PauseDispenser();
    if (!result.IsSuccess()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2811, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"paused", true}});
}

std::string TcpCommandDispatcher::HandleDispenserResume(const std::string& id, const nlohmann::json& /*params*/) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2820, "TcpDispensingFacade not available");
    }

    auto result = dispensingFacade_->ResumeDispenser();
    if (!result.IsSuccess()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2821, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"resumed", true}});
}

std::string TcpCommandDispatcher::HandlePurge(const std::string& id, const nlohmann::json& params) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2830, "TcpDispensingFacade not available");
    }

    // 解析参数
    int timeoutMs = params.value("timeout_ms", 5000);

    Application::UseCases::Dispensing::Valve::PurgeDispenserRequest request;
    request.manage_supply = true;
    request.wait_for_completion = true;
    request.timeout_ms = static_cast<uint32>(timeoutMs);

    auto result = dispensingFacade_->PurgeDispenser(request);
    if (!result.IsSuccess()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2831, result.GetError().GetMessage());
    }

    auto response = result.Value();
    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"purged", response.completed}, {"timeout_ms", timeoutMs}});
}

std::string TcpCommandDispatcher::HandleSupplyOpen(const std::string& id, const nlohmann::json& /*params*/) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2840, "TcpDispensingFacade not available");
    }

    auto result = dispensingFacade_->OpenSupplyValve();
    if (!result.IsSuccess()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2841, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"supply_open", true}});
}

std::string TcpCommandDispatcher::HandleSupplyClose(const std::string& id, const nlohmann::json& /*params*/) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2850, "TcpDispensingFacade not available");
    }

    auto result = dispensingFacade_->CloseSupplyValve();
    if (!result.IsSuccess()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2851, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"supply_open", false}});
}

std::string TcpCommandDispatcher::HandleDxfLoad(const std::string& id, const nlohmann::json& params) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2900, "TcpDispensingFacade not available");
    }

    std::string filePath = params.value("filepath", "");
    if (filePath.empty()) {
        filePath = params.value("file_path", "");
    }
    if (filePath.empty()) {
        SILIGEN_LOG_WARNING("DXF加载失败: 缺少filepath参数");
        return GatewayJsonProtocol::MakeErrorResponse(id, 2901, "Missing 'filepath' parameter");
    }
    SILIGEN_LOG_INFO("收到DXF加载请求: " + filePath);

    std::vector<uint8_t> fileContent;
    std::string readError;
    if (!ReadFileToBuffer(filePath, fileContent, readError)) {
        SILIGEN_LOG_WARNING("DXF读取失败: " + readError);
        return GatewayJsonProtocol::MakeErrorResponse(id, 2902, readError);
    }

    Application::UseCases::Dispensing::DXF::DXFUploadRequest request;
    request.file_content = std::move(fileContent);
    request.original_filename = ExtractFilename(filePath);
    request.file_size = request.file_content.size();
    request.content_type = "application/dxf";

    auto uploadResult = dispensingFacade_->UploadDxf(request);
    if (!uploadResult.IsSuccess()) {
        SILIGEN_LOG_WARNING("DXF上传失败: " + uploadResult.GetError().GetMessage());
        return GatewayJsonProtocol::MakeErrorResponse(id, 2903, uploadResult.GetError().GetMessage());
    }

    auto uploadResponse = uploadResult.Value();
    DxfCache cache;
    cache.loaded = true;
    cache.filepath = uploadResponse.filepath;

    if (dispensingFacade_) {
        Application::UseCases::Dispensing::DXF::DXFPlanningRequest planRequest;
        planRequest.dxf_filepath = uploadResponse.filepath;
        planRequest.trajectory_config = Siligen::Shared::Types::TrajectoryConfig();
        auto planResult = dispensingFacade_->PlanDxf(planRequest);
        if (!planResult.IsSuccess()) {
            SILIGEN_LOG_WARNING("DXF规划失败: " + planResult.GetError().GetMessage());
            return GatewayJsonProtocol::MakeErrorResponse(id, 2904, planResult.GetError().GetMessage());
        }

        const auto& planResponse = planResult.Value();
        cache.segment_count = static_cast<uint32_t>(planResponse.segment_count);
        cache.total_length = planResponse.total_length;
        auto bounds = ComputeBounds(planResponse.trajectory_points);
        if (bounds.has_value) {
            cache.x_min = bounds.x_min;
            cache.x_max = bounds.x_max;
            cache.y_min = bounds.y_min;
            cache.y_max = bounds.y_max;
        }
    }

    {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        dxf_cache_ = cache;
        dxf_task_id_.clear();
    }
    SILIGEN_LOG_INFO("DXF加载成功: segments=" + std::to_string(cache.segment_count) +
                     ", filepath=" + cache.filepath);
    nlohmann::json resultJson = {
        {"loaded", true},
        {"segment_count", cache.segment_count},
        {"filepath", cache.filepath}
    };
    return GatewayJsonProtocol::MakeSuccessResponse(id, resultJson);
}

std::string TcpCommandDispatcher::HandleDxfExecute(const std::string& id, const nlohmann::json& params) {
    if (!dispensingFacade_) {
        SILIGEN_LOG_WARNING("DXF执行失败: ExecuteUseCase不可用");
        FlushLogs();
        return GatewayJsonProtocol::MakeErrorResponse(id, 3000, "TcpDispensingFacade not available");
    }

    bool dryRun = params.value("dry_run", false);
    const bool requireActiveRecipe = params.value("require_active_recipe", false);
    const double dispensingSpeed = ReadJsonDouble(params, "dispensing_speed_mm_s", 0.0);
    const double dryRunSpeed = ReadJsonDouble(params, "dry_run_speed_mm_s", 0.0);
    const double rapidSpeed = ReadJsonDouble(params, "rapid_speed_mm_s", 0.0);
    const bool velocityTraceEnabled = params.value("velocity_trace_enabled", false);
    const int velocityTraceIntervalMs =
        static_cast<int>(ReadJsonSizeT(params, "velocity_trace_interval_ms", 0));
    std::string velocityTracePath;
    if (params.contains("velocity_trace_path")) {
        try {
            velocityTracePath = params.at("velocity_trace_path").get<std::string>();
        } catch (...) {
            velocityTracePath.clear();
        }
    }

    if (params.contains("speed") || params.contains("dry_run_speed") || params.contains("dryRunSpeed")) {
        FlushLogs();
        return GatewayJsonProtocol::MakeErrorResponse(id, 3002, "legacy速度字段已移除，请使用*_speed_mm_s");
    }

    SILIGEN_LOG_INFO("DXF执行字段: dry_run=" + DescribeJsonField(params, "dry_run") +
                     ", dispensing_speed_mm_s=" + DescribeJsonField(params, "dispensing_speed_mm_s") +
                     ", dry_run_speed_mm_s=" + DescribeJsonField(params, "dry_run_speed_mm_s") +
                     ", rapid_speed_mm_s=" + DescribeJsonField(params, "rapid_speed_mm_s") +
                     ", velocity_guard_enabled=" + DescribeJsonField(params, "velocity_guard_enabled") +
                     ", velocity_guard_ratio=" + DescribeJsonField(params, "velocity_guard_ratio") +
                     ", velocity_guard_abs_mm_s=" + DescribeJsonField(params, "velocity_guard_abs_mm_s") +
                     ", velocity_guard_min_expected_mm_s=" + DescribeJsonField(params, "velocity_guard_min_expected_mm_s") +
                     ", velocity_guard_grace_ms=" + DescribeJsonField(params, "velocity_guard_grace_ms") +
                     ", velocity_guard_interval_ms=" + DescribeJsonField(params, "velocity_guard_interval_ms") +
                     ", velocity_guard_max_consecutive=" + DescribeJsonField(params, "velocity_guard_max_consecutive") +
                     ", velocity_guard_stop_on_violation=" + DescribeJsonField(params, "velocity_guard_stop_on_violation") +
                     ", require_active_recipe=" + DescribeJsonField(params, "require_active_recipe") +
                     ", velocity_trace_enabled=" + DescribeJsonField(params, "velocity_trace_enabled") +
                     ", velocity_trace_interval_ms=" + DescribeJsonField(params, "velocity_trace_interval_ms") +
                     ", velocity_trace_path=" + DescribeJsonField(params, "velocity_trace_path"));
    SILIGEN_LOG_INFO_FMT_HELPER(
        "DXF执行解析: dispensing_speed_mm_s=%.3f, dry_run_speed_mm_s=%.3f, rapid_speed_mm_s=%.3f, "
        "velocity_trace=%d, interval_ms=%d, trace_path=%s",
        dispensingSpeed,
        dryRunSpeed,
        rapidSpeed,
        velocityTraceEnabled ? 1 : 0,
        velocityTraceIntervalMs,
        velocityTracePath.empty() ? "(default)" : velocityTracePath.c_str());

    std::string filepath;
    {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        if (!dxf_cache_.loaded || dxf_cache_.filepath.empty()) {
            SILIGEN_LOG_WARNING("DXF执行失败: 未加载DXF");
            FlushLogs();
            return GatewayJsonProtocol::MakeErrorResponse(id, 3001, "DXF not loaded");
        }
        filepath = dxf_cache_.filepath;
    }

    if (dispensingSpeed <= 0.0 && dryRunSpeed <= 0.0) {
        FlushLogs();
        return GatewayJsonProtocol::MakeErrorResponse(id, 3002, "必须显式指定点胶速度");
    }

    if (requireActiveRecipe) {
        if (!recipeFacade_) {
            SILIGEN_LOG_WARNING("DXF执行失败: require_active_recipe=true 但 TcpRecipeFacade 不可用");
            FlushLogs();
            return GatewayJsonProtocol::MakeErrorResponse(id, 3004, "Active recipe is required before dxf.execute");
        }

        Application::UseCases::Recipes::ListRecipesRequest recipeListRequest;
        auto recipeListResult = recipeFacade_->ListRecipes(recipeListRequest);
        if (recipeListResult.IsError()) {
            SILIGEN_LOG_WARNING("DXF执行失败: 查询配方列表失败: " + recipeListResult.GetError().GetMessage());
            FlushLogs();
            return GatewayJsonProtocol::MakeErrorResponse(id, 3004, "Active recipe is required before dxf.execute");
        }

        const auto& recipes = recipeListResult.Value().recipes;
        const bool hasActiveRecipe = std::any_of(
            recipes.begin(),
            recipes.end(),
            [](const auto& recipe) { return !recipe.active_version_id.empty(); });
        if (!hasActiveRecipe) {
            SILIGEN_LOG_WARNING("DXF执行失败: require_active_recipe=true 但未检测到已激活配方");
            FlushLogs();
            return GatewayJsonProtocol::MakeErrorResponse(id, 3004, "Active recipe is required before dxf.execute");
        }
    }

    SILIGEN_LOG_INFO("收到DXF执行请求: dry_run=" + std::string(dryRun ? "true" : "false") +
                     ", require_active_recipe=" + std::string(requireActiveRecipe ? "true" : "false") +
                     ", dispensing_speed_mm_s=" + std::to_string(dispensingSpeed) +
                     ", dry_run_speed_mm_s=" + std::to_string(dryRunSpeed) +
                     ", rapid_speed_mm_s=" + std::to_string(rapidSpeed) +
                     ", filepath=" + filepath);

    Application::UseCases::Dispensing::DXF::DXFDispensingMVPRequest request;
    request.dxf_filepath = filepath;
    request.optimize_path = params.value("optimize_path", false);
    request.start_x = params.value("start_x", 0.0f);
    request.start_y = params.value("start_y", 0.0f);
    request.approximate_splines = params.value("approximate_splines", false);
    request.two_opt_iterations = params.value("two_opt_iterations", 0);
    request.spline_max_step_mm = params.value("spline_max_step_mm", 0.0f);
    request.spline_max_error_mm = params.value("spline_max_error_mm", 0.0f);
    request.arc_tolerance_mm = static_cast<float32>(
        ReadJsonDouble(params, "arc_tolerance_mm", ReadJsonDouble(params, "arc_tolerance", 0.01)));
    request.continuity_tolerance_mm = static_cast<float32>(
        ReadJsonDouble(params, "continuity_tolerance_mm", ReadJsonDouble(params, "continuity_tolerance", 0.0)));
    request.curve_chain_angle_deg = static_cast<float32>(ReadJsonDouble(params, "curve_chain_angle_deg", 0.0));
    request.curve_chain_max_segment_mm =
        static_cast<float32>(ReadJsonDouble(params, "curve_chain_max_segment_mm", 0.0));
    request.use_hardware_trigger = params.value("use_hardware_trigger", true);
    request.dry_run = dryRun;
    if (dispensingSpeed > 0.0) {
        request.dispensing_speed_mm_s = static_cast<float32>(dispensingSpeed);
    }
    if (dryRunSpeed > 0.0) {
        request.dry_run_speed_mm_s = static_cast<float32>(dryRunSpeed);
    }
    if (rapidSpeed > 0.0) {
        request.rapid_speed_mm_s = static_cast<float32>(rapidSpeed);
    }
    request.velocity_trace_enabled = velocityTraceEnabled;
    request.velocity_trace_interval_ms = velocityTraceIntervalMs;
    request.velocity_trace_path = velocityTracePath;
    request.velocity_guard_enabled = params.value("velocity_guard_enabled", request.velocity_guard_enabled);
    request.velocity_guard_ratio =
        static_cast<float32>(ReadJsonDouble(params, "velocity_guard_ratio", request.velocity_guard_ratio));
    request.velocity_guard_abs_mm_s =
        static_cast<float32>(ReadJsonDouble(params, "velocity_guard_abs_mm_s", request.velocity_guard_abs_mm_s));
    request.velocity_guard_min_expected_mm_s = static_cast<float32>(
        ReadJsonDouble(params, "velocity_guard_min_expected_mm_s", request.velocity_guard_min_expected_mm_s));
    request.velocity_guard_grace_ms = static_cast<int32>(
        ReadJsonDouble(params, "velocity_guard_grace_ms", request.velocity_guard_grace_ms));
    request.velocity_guard_interval_ms = static_cast<int32>(
        ReadJsonDouble(params, "velocity_guard_interval_ms", request.velocity_guard_interval_ms));
    request.velocity_guard_max_consecutive = static_cast<int32>(
        ReadJsonDouble(params, "velocity_guard_max_consecutive", request.velocity_guard_max_consecutive));
    request.velocity_guard_stop_on_violation =
        params.value("velocity_guard_stop_on_violation", request.velocity_guard_stop_on_violation);

    SILIGEN_LOG_INFO_FMT_HELPER(
        "DXF执行参数: optimize=%d, start=(%.3f,%.3f), approx_splines=%d, two_opt=%d, spline_step=%.3f, "
        "spline_error=%.3f, continuity_tol=%.3f, curve_angle=%.3f, curve_seg=%.3f, hw_trigger=%d, dry_run=%d, "
        "dispensing_speed_mm_s=%.3f, dry_run_speed_mm_s=%.3f, rapid_speed_mm_s=%.3f, velocity_guard=%d, "
        "guard_ratio=%.2f, guard_abs=%.2f, guard_min_expected=%.2f, guard_grace_ms=%d, guard_interval_ms=%d, "
        "guard_max_consecutive=%d, guard_stop=%d, velocity_trace=%d, trace_interval_ms=%d",
        request.optimize_path ? 1 : 0,
        request.start_x,
        request.start_y,
        request.approximate_splines ? 1 : 0,
        request.two_opt_iterations,
        request.spline_max_step_mm,
        request.spline_max_error_mm,
        request.continuity_tolerance_mm,
        request.curve_chain_angle_deg,
        request.curve_chain_max_segment_mm,
        request.use_hardware_trigger ? 1 : 0,
        request.dry_run ? 1 : 0,
        dispensingSpeed,
        dryRunSpeed,
        rapidSpeed,
        request.velocity_guard_enabled ? 1 : 0,
        request.velocity_guard_ratio,
        request.velocity_guard_abs_mm_s,
        request.velocity_guard_min_expected_mm_s,
        request.velocity_guard_grace_ms,
        request.velocity_guard_interval_ms,
        request.velocity_guard_max_consecutive,
        request.velocity_guard_stop_on_violation ? 1 : 0,
        request.velocity_trace_enabled ? 1 : 0,
        request.velocity_trace_interval_ms);

    auto executeResult = dispensingFacade_->ExecuteDxfAsync(request);
    if (!executeResult.IsSuccess()) {
        SILIGEN_LOG_WARNING("DXF执行启动失败: " + executeResult.GetError().GetMessage());
        FlushLogs();
        return GatewayJsonProtocol::MakeErrorResponse(id, 3003, executeResult.GetError().GetMessage());
    }

    const auto& taskId = executeResult.Value();
    {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        dxf_task_id_ = taskId;
    }

    SILIGEN_LOG_INFO("DXF执行已启动, task_id=" + taskId);
    FlushLogs();
    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"executing", true}, {"task_id", taskId}});
}

std::string TcpCommandDispatcher::HandleDxfStop(const std::string& id, const nlohmann::json& /*params*/) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3030, "TcpDispensingFacade not available");
    }

    std::string taskId;
    {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        taskId = dxf_task_id_;
        dxf_task_id_.clear();
    }

    if (!taskId.empty()) {
        dispensingFacade_->CancelDxfTask(taskId);
    }
    dispensingFacade_->StopDxfExecution();

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"stopped", true}});
}

std::string TcpCommandDispatcher::HandleDxfPause(const std::string& id, const nlohmann::json& /*params*/) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3031, "TcpDispensingFacade not available");
    }

    std::string taskId;
    {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        taskId = dxf_task_id_;
    }
    if (taskId.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3032, "DXF task not running");
    }

    auto pause_result = dispensingFacade_->PauseDxfTask(taskId);
    if (pause_result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3033, pause_result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"paused", true}, {"task_id", taskId}});
}

std::string TcpCommandDispatcher::HandleDxfResume(const std::string& id, const nlohmann::json& /*params*/) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3034, "TcpDispensingFacade not available");
    }

    std::string taskId;
    {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        taskId = dxf_task_id_;
    }
    if (taskId.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3035, "DXF task not paused");
    }

    auto resume_result = dispensingFacade_->ResumeDxfTask(taskId);
    if (resume_result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3036, resume_result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"resumed", true}, {"task_id", taskId}});
}

std::string TcpCommandDispatcher::HandleDxfInfo(const std::string& id, const nlohmann::json& /*params*/) {
    DxfCache cache;
    {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        cache = dxf_cache_;
    }

    if (!cache.loaded || cache.filepath.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3010, "DXF not loaded");
    }

    nlohmann::json boundsJson = {
        {"x_min", cache.x_min},
        {"x_max", cache.x_max},
        {"y_min", cache.y_min},
        {"y_max", cache.y_max}
    };

    nlohmann::json resultJson = {
        {"total_length", cache.total_length},
        {"bounds", boundsJson}
    };
    return GatewayJsonProtocol::MakeSuccessResponse(id, resultJson);
}

std::string TcpCommandDispatcher::HandleDxfProgress(const std::string& id, const nlohmann::json& /*params*/) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3020, "TcpDispensingFacade not available");
    }

    std::string taskId;
    {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        taskId = dxf_task_id_;
    }

    if (taskId.empty()) {
        return GatewayJsonProtocol::MakeSuccessResponse(id, {
            {"running", false},
            {"progress", 0},
            {"current_segment", 0},
            {"total_segments", 0},
            {"state", "idle"},
            {"error_message", ""}
        });
    }

    auto statusResult = dispensingFacade_->GetDxfTaskStatus(taskId);
    if (!statusResult.IsSuccess()) {
        const auto& err = statusResult.GetError();
        return GatewayJsonProtocol::MakeSuccessResponse(id, {
            {"running", false},
            {"progress", 0},
            {"current_segment", 0},
            {"total_segments", 0},
            {"state", "unknown"},
            {"error_message", err.GetMessage()}
        });
    }

    const auto& status = statusResult.Value();
    const auto& state = status.state;
    bool running = (state == "running" || state == "pending" || state == "paused");
    int progress = static_cast<int>(status.progress_percent);
    if (state == "completed") {
        progress = 100;
    }
    if (state == "completed" || state == "failed" || state == "cancelled") {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        dxf_task_id_.clear();
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {
        {"running", running},
        {"progress", progress},
        {"current_segment", status.executed_segments},
        {"total_segments", status.total_segments},
        {"state", state},
        {"error_message", status.error_message}
    });
}

std::string TcpCommandDispatcher::HandleAlarmsList(const std::string& id, const nlohmann::json& /*params*/) {
    nlohmann::json alarm_list = nlohmann::json::array();

    if (motionFacade_) {
        struct AxisInfo {
            LogicalAxisId axis;
            const char* name;
        };
        const AxisInfo axes[] = {
            {LogicalAxisId::X, "X"},
            {LogicalAxisId::Y, "Y"},
            {LogicalAxisId::Z, "Z"},
            {LogicalAxisId::U, "U"}
        };

        for (const auto& axis_info : axes) {
            auto servo_result = motionFacade_->ReadServoAlarmStatus(axis_info.axis);
            if (servo_result.IsSuccess() && servo_result.Value()) {
                std::string alarm_id = std::string("SERVO_") + axis_info.name;
                alarm_list.push_back({
                    {"id", alarm_id},
                    {"level", "CRIT"},
                    {"message", std::string(axis_info.name) + "轴伺服报警"}
                });
            }

            auto limit_pos = motionFacade_->ReadLimitStatus(axis_info.axis, true);
            if (limit_pos.IsSuccess() && limit_pos.Value()) {
                std::string alarm_id = std::string("LIMIT_") + axis_info.name + "_POS";
                alarm_list.push_back({
                    {"id", alarm_id},
                    {"level", "WARN"},
                    {"message", std::string(axis_info.name) + "轴正限位触发"}
                });
            }

            auto limit_neg = motionFacade_->ReadLimitStatus(axis_info.axis, false);
            if (limit_neg.IsSuccess() && limit_neg.Value()) {
                std::string alarm_id = std::string("LIMIT_") + axis_info.name + "_NEG";
                alarm_list.push_back({
                    {"id", alarm_id},
                    {"level", "WARN"},
                    {"message", std::string(axis_info.name) + "轴负限位触发"}
                });
            }
        }
    }

    std::unordered_set<std::string> acknowledged;
    {
        std::lock_guard<std::mutex> lock(alarms_mutex_);
        acknowledged = acknowledged_alarm_ids_;
    }

    if (!acknowledged.empty() && !alarm_list.empty()) {
        nlohmann::json filtered = nlohmann::json::array();
        for (const auto& alarm : alarm_list) {
            auto id_iter = alarm.find("id");
            if (id_iter == alarm.end()) {
                continue;
            }
            const std::string alarm_id = id_iter->get<std::string>();
            if (acknowledged.count(alarm_id) == 0) {
                filtered.push_back(alarm);
            }
        }
        alarm_list = std::move(filtered);
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"alarms", alarm_list}});
}

std::string TcpCommandDispatcher::HandleAlarmsClear(const std::string& id, const nlohmann::json& /*params*/) {
    {
        std::lock_guard<std::mutex> lock(alarms_mutex_);
        acknowledged_alarm_ids_.clear();
    }
    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"ok", true}});
}

std::string TcpCommandDispatcher::HandleAlarmsAcknowledge(const std::string& id, const nlohmann::json& params) {
    std::string alarm_id = params.value("id", "");
    if (alarm_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3201, "Missing 'id' parameter");
    }
    {
        std::lock_guard<std::mutex> lock(alarms_mutex_);
        acknowledged_alarm_ids_.insert(alarm_id);
    }
    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"ok", true}});
}

std::string TcpCommandDispatcher::HandleRecipeList(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3300, "TcpRecipeFacade not available");
    }

    Application::UseCases::Recipes::ListRecipesRequest request;
    request.status = params.value("status", "");
    request.query = params.value("query", "");
    request.tag = params.value("tag", "");

    auto result = recipeFacade_->ListRecipes(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3301, result.GetError().GetMessage());
    }

    nlohmann::json recipes = nlohmann::json::array();
    for (const auto& recipe : result.Value().recipes) {
        recipes.push_back(RecipeJsonSerializer::RecipeToJson(recipe));
    }
    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"recipes", recipes}});
}

std::string TcpCommandDispatcher::HandleRecipeGet(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3310, "TcpRecipeFacade not available");
    }

    std::string recipe_id = params.value("recipeId", "");
    if (recipe_id.empty()) {
        recipe_id = params.value("recipe_id", "");
    }
    if (recipe_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3311, "Missing 'recipeId' parameter");
    }

    Application::UseCases::Recipes::GetRecipeRequest request;
    request.recipe_id = recipe_id;

    auto result = recipeFacade_->GetRecipe(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3312, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"recipe", RecipeJsonSerializer::RecipeToJson(result.Value().recipe)}});
}

std::string TcpCommandDispatcher::HandleRecipeCreate(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3320, "TcpRecipeFacade not available");
    }

    std::string name = params.value("name", "");
    if (name.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3321, "Missing 'name' parameter");
    }

    Application::UseCases::Recipes::CreateRecipeRequest request;
    request.name = name;
    request.description = params.value("description", "");
    request.tags = ParseTags(params);
    request.actor = params.value("actor", "");

    auto result = recipeFacade_->CreateRecipe(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3322, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"recipe", RecipeJsonSerializer::RecipeToJson(result.Value().recipe)}});
}

std::string TcpCommandDispatcher::HandleRecipeUpdate(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3330, "TcpRecipeFacade not available");
    }

    std::string recipe_id = params.value("recipeId", "");
    if (recipe_id.empty()) {
        recipe_id = params.value("recipe_id", "");
    }
    if (recipe_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3331, "Missing 'recipeId' parameter");
    }

    Application::UseCases::Recipes::UpdateRecipeRequest request;
    request.recipe_id = recipe_id;
    if (params.contains("name")) {
        request.name = params.value("name", "");
        request.update_name = true;
    }
    if (params.contains("description")) {
        request.description = params.value("description", "");
        request.update_description = true;
    }
    if (params.contains("tags")) {
        request.tags = ParseTags(params);
        request.update_tags = true;
    }
    request.actor = params.value("actor", "");

    auto result = recipeFacade_->UpdateRecipe(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3332, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"recipe", RecipeJsonSerializer::RecipeToJson(result.Value().recipe)}});
}

std::string TcpCommandDispatcher::HandleRecipeArchive(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3340, "TcpRecipeFacade not available");
    }

    std::string recipe_id = params.value("recipeId", "");
    if (recipe_id.empty()) {
        recipe_id = params.value("recipe_id", "");
    }
    if (recipe_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3341, "Missing 'recipeId' parameter");
    }

    Application::UseCases::Recipes::ArchiveRecipeRequest request;
    request.recipe_id = recipe_id;
    request.actor = params.value("actor", "");

    auto result = recipeFacade_->ArchiveRecipe(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3342, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"archived", result.Value().archived}});
}

std::string TcpCommandDispatcher::HandleRecipeDraftCreate(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3350, "TcpRecipeFacade not available");
    }

    std::string recipe_id = params.value("recipeId", "");
    if (recipe_id.empty()) {
        recipe_id = params.value("recipe_id", "");
    }
    std::string template_id = params.value("templateId", "");
    if (template_id.empty()) {
        template_id = params.value("template_id", "");
    }
    if (recipe_id.empty() || template_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3351, "Missing 'recipeId' or 'templateId' parameter");
    }

    Application::UseCases::Recipes::CreateDraftVersionRequest request;
    request.recipe_id = recipe_id;
    request.template_id = template_id;
    request.base_version_id = params.value("baseVersionId", params.value("base_version_id", ""));
    request.version_label = params.value("versionLabel", params.value("version_label", ""));
    request.change_note = params.value("changeNote", params.value("change_note", ""));
    request.created_by = params.value("actor", "");

    auto result = recipeFacade_->CreateDraft(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3352, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"version", RecipeJsonSerializer::RecipeVersionToJson(result.Value().version)}});
}

std::string TcpCommandDispatcher::HandleRecipeDraftUpdate(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3360, "TcpRecipeFacade not available");
    }

    std::string recipe_id = params.value("recipeId", "");
    if (recipe_id.empty()) {
        recipe_id = params.value("recipe_id", "");
    }
    std::string version_id = params.value("versionId", "");
    if (version_id.empty()) {
        version_id = params.value("version_id", "");
    }
    if (recipe_id.empty() || version_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3361, "Missing 'recipeId' or 'versionId' parameter");
    }

    std::vector<ParameterValueEntry> parameters;
    std::string parse_error;
    if (!ParseParameterEntries(params, parameters, parse_error)) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3362, parse_error);
    }

    Application::UseCases::Recipes::UpdateDraftVersionRequest request;
    request.recipe_id = recipe_id;
    request.version_id = version_id;
    request.parameters = parameters;
    request.change_note = params.value("changeNote", params.value("change_note", ""));
    request.editor = params.value("actor", "");

    auto result = recipeFacade_->UpdateDraft(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3363, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"version", RecipeJsonSerializer::RecipeVersionToJson(result.Value().version)}});
}

std::string TcpCommandDispatcher::HandleRecipePublish(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3370, "TcpRecipeFacade not available");
    }

    std::string recipe_id = params.value("recipeId", "");
    if (recipe_id.empty()) {
        recipe_id = params.value("recipe_id", "");
    }
    std::string version_id = params.value("versionId", "");
    if (version_id.empty()) {
        version_id = params.value("version_id", "");
    }
    if (recipe_id.empty() || version_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3371, "Missing 'recipeId' or 'versionId' parameter");
    }

    Application::UseCases::Recipes::PublishRecipeVersionRequest request;
    request.recipe_id = recipe_id;
    request.version_id = version_id;
    request.actor = params.value("actor", "");

    auto result = recipeFacade_->PublishRecipeVersion(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3372, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"version", RecipeJsonSerializer::RecipeVersionToJson(result.Value().version)}});
}

std::string TcpCommandDispatcher::HandleRecipeVersions(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3380, "TcpRecipeFacade not available");
    }

    std::string recipe_id = params.value("recipeId", "");
    if (recipe_id.empty()) {
        recipe_id = params.value("recipe_id", "");
    }
    if (recipe_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3381, "Missing 'recipeId' parameter");
    }

    Application::UseCases::Recipes::ListRecipeVersionsRequest request;
    request.recipe_id = recipe_id;

    auto result = recipeFacade_->ListRecipeVersions(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3382, result.GetError().GetMessage());
    }

    nlohmann::json versions = nlohmann::json::array();
    for (const auto& version : result.Value().versions) {
        versions.push_back(RecipeJsonSerializer::RecipeVersionToJson(version));
    }
    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"versions", versions}});
}

std::string TcpCommandDispatcher::HandleRecipeVersionCreate(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3390, "TcpRecipeFacade not available");
    }

    std::string recipe_id = params.value("recipeId", "");
    if (recipe_id.empty()) {
        recipe_id = params.value("recipe_id", "");
    }
    if (recipe_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3391, "Missing 'recipeId' parameter");
    }

    Application::UseCases::Recipes::CreateVersionFromPublishedRequest request;
    request.recipe_id = recipe_id;
    request.base_version_id = params.value("baseVersionId", params.value("base_version_id", ""));
    request.version_label = params.value("versionLabel", params.value("version_label", ""));
    request.change_note = params.value("changeNote", params.value("change_note", ""));
    request.created_by = params.value("actor", "");

    auto result = recipeFacade_->CreateVersionFromPublished(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3392, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"version", RecipeJsonSerializer::RecipeVersionToJson(result.Value().version)}});
}

std::string TcpCommandDispatcher::HandleRecipeCompare(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3400, "TcpRecipeFacade not available");
    }

    std::string recipe_id = params.value("recipeId", "");
    if (recipe_id.empty()) {
        recipe_id = params.value("recipe_id", "");
    }
    std::string base_version_id = params.value("baseVersionId", params.value("base_version_id", ""));
    std::string version_id = params.value("versionId", params.value("version_id", ""));
    if (recipe_id.empty() || base_version_id.empty() || version_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3401, "Missing 'recipeId', 'baseVersionId', or 'versionId' parameter");
    }

    Application::UseCases::Recipes::CompareRecipeVersionsRequest request;
    request.recipe_id = recipe_id;
    request.from_version_id = base_version_id;
    request.to_version_id = version_id;

    auto result = recipeFacade_->CompareRecipeVersions(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3402, result.GetError().GetMessage());
    }

    nlohmann::json changes = nlohmann::json::array();
    for (const auto& change : result.Value().changes) {
        changes.push_back(RecipeJsonSerializer::FieldChangeToJson(change));
    }
    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"changes", changes}});
}

std::string TcpCommandDispatcher::HandleRecipeActivate(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3410, "TcpRecipeFacade not available");
    }

    std::string recipe_id = params.value("recipeId", "");
    if (recipe_id.empty()) {
        recipe_id = params.value("recipe_id", "");
    }
    std::string version_id = params.value("versionId", "");
    if (version_id.empty()) {
        version_id = params.value("version_id", "");
    }
    if (recipe_id.empty() || version_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3411, "Missing 'recipeId' or 'versionId' parameter");
    }

    Application::UseCases::Recipes::ActivateRecipeVersionRequest request;
    request.recipe_id = recipe_id;
    request.version_id = version_id;
    request.actor = params.value("actor", "");

    auto result = recipeFacade_->ActivateRecipeVersion(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3412, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"activated", true}});
}

std::string TcpCommandDispatcher::HandleRecipeTemplates(const std::string& id, const nlohmann::json& /*params*/) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3420, "TcpRecipeFacade not available");
    }

    Application::UseCases::Recipes::ListTemplatesRequest request;
    auto result = recipeFacade_->ListTemplates(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3421, result.GetError().GetMessage());
    }

    nlohmann::json templates = nlohmann::json::array();
    for (const auto& tmpl : result.Value().templates) {
        templates.push_back(RecipeJsonSerializer::TemplateToJson(tmpl));
    }
    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"templates", templates}});
}

std::string TcpCommandDispatcher::HandleRecipeSchemaDefault(const std::string& id, const nlohmann::json& /*params*/) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3430, "TcpRecipeFacade not available");
    }

    Application::UseCases::Recipes::GetDefaultParameterSchemaRequest request;
    auto result = recipeFacade_->GetDefaultParameterSchema(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3431, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"schema", RecipeJsonSerializer::ParameterSchemaToJson(result.Value().schema)}});
}

std::string TcpCommandDispatcher::HandleRecipeAudit(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3440, "TcpRecipeFacade not available");
    }

    std::string recipe_id = params.value("recipeId", "");
    if (recipe_id.empty()) {
        recipe_id = params.value("recipe_id", "");
    }
    if (recipe_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3441, "Missing 'recipeId' parameter");
    }

    Application::UseCases::Recipes::GetRecipeAuditRequest request;
    request.recipe_id = recipe_id;
    std::string version_id = params.value("versionId", params.value("version_id", ""));
    if (!version_id.empty()) {
        request.version_id = version_id;
    }

    auto result = recipeFacade_->GetRecipeAudit(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3442, result.GetError().GetMessage());
    }

    nlohmann::json records = nlohmann::json::array();
    for (const auto& record : result.Value().records) {
        records.push_back(RecipeJsonSerializer::AuditRecordToJson(record));
    }
    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"records", records}});
}

std::string TcpCommandDispatcher::HandleRecipeExport(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3450, "TcpRecipeFacade not available");
    }

    std::string recipe_id = params.value("recipeId", "");
    if (recipe_id.empty()) {
        recipe_id = params.value("recipe_id", "");
    }
    if (recipe_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3451, "Missing 'recipeId' parameter");
    }

    Application::UseCases::Recipes::ExportRecipeBundlePayloadRequest request;
    request.recipe_id = recipe_id;
    const std::string output_path = params.value("outputPath", params.value("output_path", ""));

    auto result = recipeFacade_->ExportBundle(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3452, result.GetError().GetMessage());
    }

    if (!output_path.empty()) {
        std::ofstream out(output_path, std::ios::binary | std::ios::trunc);
        if (!out) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 3453, "Failed to write export file");
        }
        out << result.Value().bundle_json;
        if (!out) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 3454, "Failed to write export file");
        }
    }

    nlohmann::json response = {
        {"outputPath", output_path},
        {"recipeCount", result.Value().recipe_count},
        {"versionCount", result.Value().version_count},
        {"auditCount", result.Value().audit_count}
    };
    if (output_path.empty()) {
        response["bundleJson"] = result.Value().bundle_json;
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, response);
}

std::string TcpCommandDispatcher::HandleRecipeImport(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3460, "TcpRecipeFacade not available");
    }

    std::string bundle_json = params.value("bundleJson", params.value("bundle_json", ""));
    std::string bundle_path = params.value("bundlePath", params.value("bundle_path", ""));
    if (bundle_json.empty() && bundle_path.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3461, "Missing bundle payload");
    }

    if (bundle_json.empty() && !bundle_path.empty()) {
        std::ifstream in(bundle_path, std::ios::binary);
        if (!in) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 3463, "Bundle file not found");
        }
        std::ostringstream buffer;
        buffer << in.rdbuf();
        bundle_json = buffer.str();
    }

    Application::UseCases::Recipes::ImportRecipeBundlePayloadRequest request;
    request.bundle_json = bundle_json;
    request.resolution_strategy = params.value("resolution", "");
    request.dry_run = params.value("dryRun", params.value("dry_run", false));
    request.actor = params.value("actor", "");
    request.source_label = bundle_path;

    auto result = recipeFacade_->ImportBundle(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3462, result.GetError().GetMessage());
    }

    nlohmann::json conflicts = nlohmann::json::array();
    for (const auto& conflict : result.Value().conflicts) {
        conflicts.push_back(ImportConflictToJson(conflict));
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {
        {"status", result.Value().status},
        {"importedCount", result.Value().imported_count},
        {"conflicts", conflicts}
    });
}

} // namespace Siligen::Adapters::Tcp





