#pragma once

#include "siligen/shared/numeric_types.h"

#include <chrono>
#include <string>

namespace Siligen::SharedKernel {

enum class ErrorCode : int32 {
    SUCCESS = 0,
    INVALID_PARAMETER = 1,
    INVALID_STATE = 2,
    COMMAND_FAILED = 3,
    CONNECTION_FAILED = 4,

    HARDWARE_NOT_CONNECTED = 1000,
    HARDWARE_CONNECTION_FAILED = 1001,
    AXIS_NOT_FOUND = 1002,
    INVALID_POSITION = 1003,
    EMERGENCY_STOP_ACTIVATED = 1004,
    MOTION_TIMEOUT = 1005,
    AXIS_NOT_HOMED = 1006,
    POSITION_OUT_OF_RANGE = 1007,
    VELOCITY_LIMIT_EXCEEDED = 1008,
    HARDWARE_ERROR = 1009,
    INVALID_AXIS = 1010,
    HARDWARE_TIMEOUT = 1011,
    HARDWARE_NOT_RESPONDING = 1012,
    HARDWARE_COMMAND_FAILED = 1013,
    DISPENSER_TRIGGER_INCOMPLETE = 1014,

    CONFIGURATION_ERROR = 2001,
    LOG_INITIALIZATION_FAILED = 2002,
    CONFIG_FILE_NOT_FOUND = 2003,
    INVALID_CONFIG_VALUE = 2004,
    CONFIG_PARSE_ERROR = 2005,
    VALIDATION_FAILED = 2006,

    REPOSITORY_NOT_AVAILABLE = 300,
    DATABASE_WRITE_FAILED = 301,
    DATA_SERIALIZATION_FAILED = 302,
    NOT_FOUND = 303,
    RECIPE_NOT_FOUND = 310,
    RECIPE_VERSION_NOT_FOUND = 311,
    RECIPE_ALREADY_EXISTS = 312,
    RECIPE_INVALID_STATE = 313,
    RECIPE_VALIDATION_FAILED = 314,

    FILE_PARSING_FAILED = 400,
    PATH_OPTIMIZATION_FAILED = 401,
    FILE_VALIDATION_FAILED = 402,

    JSON_PARSE_ERROR = 6000,
    JSON_MISSING_REQUIRED_FIELD = 6001,
    JSON_INVALID_TYPE = 6002,
    SERIALIZATION_FAILED = 6003,
    DESERIALIZATION_FAILED = 6004,

    DUPLICATE_DECISION_ID = 7104,

    ADAPTER_EXCEPTION = 9000,
    ADAPTER_NOT_INITIALIZED = 9001,
    ADAPTER_UNKNOWN_EXCEPTION = 9002,
    NOT_IMPLEMENTED = 9003,
    TIMEOUT = 9004,
    MOTION_ERROR = 9005,

    NETWORK_CONNECTION_FAILED = 9101,
    NETWORK_TIMEOUT = 9102,
    NETWORK_ERROR = 9199,

    PORT_NOT_INITIALIZED = 102,

    UNKNOWN_ERROR = 9999,
};

class Error {
   public:
    Error(ErrorCode code, std::string message, std::string module = {})
        : code_(code),
          message_(std::move(message)),
          module_(std::move(module)),
          timestamp_(std::chrono::system_clock::now()) {}

    Error()
        : code_(ErrorCode::SUCCESS),
          timestamp_(std::chrono::system_clock::now()) {}

    [[nodiscard]] ErrorCode GetCode() const { return code_; }
    [[nodiscard]] const std::string& GetMessage() const { return message_; }
    [[nodiscard]] const std::string& GetModule() const { return module_; }
    [[nodiscard]] const std::chrono::system_clock::time_point& GetTimestamp() const { return timestamp_; }

    [[nodiscard]] std::string ToString() const {
        std::string result = "[ERROR]";
        if (!module_.empty()) {
            result += " " + module_ + ":";
        }
        result += " " + message_;
        result += " (错误码: " + std::to_string(static_cast<int32>(code_)) + ")";
        return result;
    }

    [[nodiscard]] bool IsSuccess() const { return code_ == ErrorCode::SUCCESS; }
    [[nodiscard]] bool IsError() const { return code_ != ErrorCode::SUCCESS; }

   private:
    ErrorCode code_;
    std::string message_;
    std::string module_;
    std::chrono::system_clock::time_point timestamp_;
};

}  // namespace Siligen::SharedKernel
