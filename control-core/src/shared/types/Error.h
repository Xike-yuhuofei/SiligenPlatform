// Error.h - 基础错误类型和错误代码 (Error types and error codes)
// Task: T013 - Phase 2 基础设施 - 共享类型系统
#pragma once

#ifdef _WIN32
// Undefine Windows macro that conflicts with Error::GetMessage()
#ifdef GetMessage
#undef GetMessage
#endif
#endif

#include <chrono>
#include <cstdint>
#include <string>

namespace Siligen {
namespace Shared {
namespace Types {

// 基础类型定义 (Basic type definitions)
using int32 = std::int32_t;
using float32 = float;

// 错误代码枚举 (Error code enumeration)
enum class ErrorCode : int32 {
    SUCCESS = 0,

    // 通用错误 (General errors 1-99)
    INVALID_PARAMETER = 1,
    INVALID_STATE = 2,
    COMMAND_FAILED = 3,
    CONNECTION_FAILED = 4,  // 通用连接失败(历史保留,优先使用PORT_NOT_INITIALIZED/NETWORK_CONNECTION_FAILED)

    // 硬件错误 (Hardware errors 1000-1999)
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
    HARDWARE_TIMEOUT = 1011,         // 硬件响应超时
    HARDWARE_NOT_RESPONDING = 1012,  // 硬件无响应
    HARDWARE_COMMAND_FAILED = 1013,  // 硬件命令执行失败

    // 配置错误 (Configuration errors 2000-2999)
    CONFIGURATION_ERROR = 2001,
    LOG_INITIALIZATION_FAILED = 2002,
    CONFIG_FILE_NOT_FOUND = 2003,
    INVALID_CONFIG_VALUE = 2004,
    CONFIG_PARSE_ERROR = 2005,
    ValidationFailed = 2006,

    // 文件上传错误 (File upload errors 2500-2599)
    FILE_UPLOAD_FAILED = 2501,
    FILE_SIZE_EXCEEDED = 2502,
    FILE_TYPE_INVALID = 2503,
    FILE_FORMAT_INVALID = 2504,
    FILE_IO_ERROR = 2505,
    FILE_NAME_INVALID = 2506,
    FILE_ALREADY_EXISTS = 2507,
    DISK_SPACE_INSUFFICIENT = 2508,
    FILE_PATH_INVALID = 2509,
    FILE_NOT_FOUND = 2510,
    FILE_DELETE_FAILED = 2511,
    MULTIPART_PARSE_ERROR = 2512,

    // 依赖注入错误 (Dependency injection errors 3000-3999)
    SERVICE_NOT_REGISTERED = 3001,
    CIRCULAR_DEPENDENCY_DETECTED = 3002,
    SERVICE_INITIALIZATION_FAILED = 3003,
    INVALID_SERVICE_LIFETIME = 3004,

    // CMP触发错误 (CMP trigger errors 4000-4999)
    CMP_CONFIGURATION_INVALID = 4001,
    CMP_TRIGGER_SETUP_FAILED = 4002,
    CMP_POSITION_OUT_OF_RANGE = 4003,
    TRAJECTORY_GENERATION_FAILED = 4004,

    // 序列化错误 (Serialization errors 6000-6999)
    JSON_PARSE_ERROR = 6000,
    JSON_MISSING_REQUIRED_FIELD = 6001,
    JSON_INVALID_TYPE = 6002,
    SERIALIZATION_FAILED = 6003,
    DESERIALIZATION_FAILED = 6004,
    BATCH_SERIALIZATION_FAILED = 6005,
    BATCH_DESERIALIZATION_FAILED = 6006,
    OBJECT_CONSTRUCTION_FAILED = 6007,
    ENUM_CONVERSION_FAILED = 6008,
    UNSUPPORTED_TYPE = 6009,

    // 适配器错误 (Adapter errors 9000-9099)
    ADAPTER_EXCEPTION = 9000,          // 适配器内部异常
    ADAPTER_NOT_INITIALIZED = 9001,    // 适配器未初始化
    ADAPTER_UNKNOWN_EXCEPTION = 9002,  // 适配器未知异常
    NOT_IMPLEMENTED = 9003,            // 功能未实现
    TIMEOUT = 9004,                    // 操作超时
    MOTION_ERROR = 9005,               // 运动错误

    // 网络错误 (Network errors 9100-9199)
    NETWORK_CONNECTION_FAILED = 9101,  // 网络连接失败
    NETWORK_TIMEOUT = 9102,            // 网络超时
    NETWORK_ERROR = 9199,              // 网络错误

    // 端口状态错误 (Port state errors 100-199)
    PORT_NOT_INITIALIZED = 102,  // 连接端口未初始化

    // 运动控制错误 (Motion control errors 200-299)
    MOTION_START_FAILED = 200,        // 运动启动失败
    MOTION_STOP_FAILED = 201,         // 运动停止失败
    POSITION_QUERY_FAILED = 202,      // 位置查询失败
    HARDWARE_OPERATION_FAILED = 203,  // 硬件操作失败

    // 数据持久化错误 (Data persistence errors 300-399)
    REPOSITORY_NOT_AVAILABLE = 300,   // 数据仓库不可用
    DATABASE_WRITE_FAILED = 301,      // 数据库写入失败
    DATA_SERIALIZATION_FAILED = 302,  // 数据序列化失败
    NOT_FOUND = 303,                  // 资源未找到
    RECIPE_NOT_FOUND = 310,           // 配方未找到
    RECIPE_VERSION_NOT_FOUND = 311,   // 配方版本未找到
    RECIPE_ALREADY_EXISTS = 312,      // 配方已存在
    RECIPE_INVALID_STATE = 313,       // 配方状态非法
    RECIPE_VALIDATION_FAILED = 314,   // 配方校验失败
    RECIPE_IMPORT_CONFLICT = 315,     // 配方导入冲突
    TEMPLATE_NOT_FOUND = 316,         // 模板未找到
    PARAMETER_SCHEMA_NOT_FOUND = 317, // 参数模式未找到

    // 文件解析错误 (File parsing errors 400-499)
    FILE_PARSING_FAILED = 400,       // 文件解析失败
    PATH_OPTIMIZATION_FAILED = 401,  // 路径优化失败
    FILE_VALIDATION_FAILED = 402,    // 文件验证失败

    // 线程操作错误 (Thread operation errors 500-599)
    THREAD_START_FAILED = 500,  // 线程启动失败

    // 兜底错误 (Fallback errors 9900-9999)
    UNKNOWN_ERROR = 9999  // 未知错误 (应避免使用)
};

// 错误信息类 (Error information class)
class Error {
   private:
    ErrorCode code_;
    std::string message_;
    std::string module_;
    std::chrono::system_clock::time_point timestamp_;

   public:
    // 构造函数 (Constructor)
    Error(ErrorCode code, const std::string& message, const std::string& module = "")
        : code_(code), message_(message), module_(module), timestamp_(std::chrono::system_clock::now()) {}

    // 默认构造函数 (Default constructor)
    Error() : code_(ErrorCode::SUCCESS), message_(""), module_(""), timestamp_(std::chrono::system_clock::now()) {}

    // 获取错误代码 (Get error code)
    ErrorCode GetCode() const {
        return code_;
    }

    // 获取错误消息 (Get error message)
    const std::string& GetMessage() const {
        return message_;
    }

    // 获取模块名称 (Get module name)
    const std::string& GetModule() const {
        return module_;
    }

    // 获取时间戳 (Get timestamp)
    const std::chrono::system_clock::time_point& GetTimestamp() const {
        return timestamp_;
    }

    // 转换为字符串 (Convert to string)
    // 格式: [ERROR] 模块名称: 错误描述 (错误码)
    std::string ToString() const {
        std::string result = "[ERROR]";

        if (!module_.empty()) {
            result += " " + module_ + ":";
        }

        result += " " + message_;
        result += " (错误码: " + std::to_string(static_cast<int32>(code_)) + ")";

        return result;
    }

    // 判断是否为成功状态 (Check if successful)
    bool IsSuccess() const {
        return code_ == ErrorCode::SUCCESS;
    }

    // 判断是否为错误状态 (Check if error)
    bool IsError() const {
        return code_ != ErrorCode::SUCCESS;
    }
};

// 错误代码工具函数 (Error code utility functions)
inline const char* ErrorCodeToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::SUCCESS:
            return "SUCCESS";

        // 通用错误 (General errors)
        case ErrorCode::INVALID_PARAMETER:
            return "INVALID_PARAMETER";
        case ErrorCode::INVALID_STATE:
            return "INVALID_STATE";
        case ErrorCode::COMMAND_FAILED:
            return "COMMAND_FAILED";
        case ErrorCode::CONNECTION_FAILED:
            return "CONNECTION_FAILED";

        // 硬件错误 (Hardware errors)
        case ErrorCode::HARDWARE_NOT_CONNECTED:
            return "HARDWARE_NOT_CONNECTED";
        case ErrorCode::HARDWARE_CONNECTION_FAILED:
            return "HARDWARE_CONNECTION_FAILED";
        case ErrorCode::AXIS_NOT_FOUND:
            return "AXIS_NOT_FOUND";
        case ErrorCode::INVALID_POSITION:
            return "INVALID_POSITION";
        case ErrorCode::EMERGENCY_STOP_ACTIVATED:
            return "EMERGENCY_STOP_ACTIVATED";
        case ErrorCode::MOTION_TIMEOUT:
            return "MOTION_TIMEOUT";
        case ErrorCode::AXIS_NOT_HOMED:
            return "AXIS_NOT_HOMED";
        case ErrorCode::POSITION_OUT_OF_RANGE:
            return "POSITION_OUT_OF_RANGE";
        case ErrorCode::VELOCITY_LIMIT_EXCEEDED:
            return "VELOCITY_LIMIT_EXCEEDED";
        case ErrorCode::HARDWARE_ERROR:
            return "HARDWARE_ERROR";
        case ErrorCode::INVALID_AXIS:
            return "INVALID_AXIS";

        // 配置错误 (Configuration errors)
        case ErrorCode::CONFIGURATION_ERROR:
            return "CONFIGURATION_ERROR";
        case ErrorCode::LOG_INITIALIZATION_FAILED:
            return "LOG_INITIALIZATION_FAILED";
        case ErrorCode::CONFIG_FILE_NOT_FOUND:
            return "CONFIG_FILE_NOT_FOUND";
        case ErrorCode::INVALID_CONFIG_VALUE:
            return "INVALID_CONFIG_VALUE";
        case ErrorCode::CONFIG_PARSE_ERROR:
            return "CONFIG_PARSE_ERROR";
        case ErrorCode::ValidationFailed:
            return "VALIDATION_FAILED";

        // 文件上传错误 (File upload errors)
        case ErrorCode::FILE_UPLOAD_FAILED:
            return "FILE_UPLOAD_FAILED";
        case ErrorCode::FILE_SIZE_EXCEEDED:
            return "FILE_SIZE_EXCEEDED";
        case ErrorCode::FILE_TYPE_INVALID:
            return "FILE_TYPE_INVALID";
        case ErrorCode::FILE_FORMAT_INVALID:
            return "FILE_FORMAT_INVALID";
        case ErrorCode::FILE_IO_ERROR:
            return "FILE_IO_ERROR";
        case ErrorCode::FILE_NAME_INVALID:
            return "FILE_NAME_INVALID";
        case ErrorCode::FILE_ALREADY_EXISTS:
            return "FILE_ALREADY_EXISTS";
        case ErrorCode::DISK_SPACE_INSUFFICIENT:
            return "DISK_SPACE_INSUFFICIENT";
        case ErrorCode::FILE_PATH_INVALID:
            return "FILE_PATH_INVALID";
        case ErrorCode::FILE_NOT_FOUND:
            return "FILE_NOT_FOUND";
        case ErrorCode::FILE_DELETE_FAILED:
            return "FILE_DELETE_FAILED";
        case ErrorCode::MULTIPART_PARSE_ERROR:
            return "MULTIPART_PARSE_ERROR";

        // 依赖注入错误 (Dependency injection errors)
        case ErrorCode::SERVICE_NOT_REGISTERED:
            return "SERVICE_NOT_REGISTERED";
        case ErrorCode::CIRCULAR_DEPENDENCY_DETECTED:
            return "CIRCULAR_DEPENDENCY_DETECTED";
        case ErrorCode::SERVICE_INITIALIZATION_FAILED:
            return "SERVICE_INITIALIZATION_FAILED";
        case ErrorCode::INVALID_SERVICE_LIFETIME:
            return "INVALID_SERVICE_LIFETIME";

        // CMP触发错误 (CMP trigger errors)
        case ErrorCode::CMP_CONFIGURATION_INVALID:
            return "CMP_CONFIGURATION_INVALID";
        case ErrorCode::CMP_TRIGGER_SETUP_FAILED:
            return "CMP_TRIGGER_SETUP_FAILED";
        case ErrorCode::CMP_POSITION_OUT_OF_RANGE:
            return "CMP_POSITION_OUT_OF_RANGE";
        case ErrorCode::TRAJECTORY_GENERATION_FAILED:
            return "TRAJECTORY_GENERATION_FAILED";

        // 序列化错误 (Serialization errors)
        case ErrorCode::JSON_PARSE_ERROR:
            return "JSON_PARSE_ERROR";
        case ErrorCode::JSON_MISSING_REQUIRED_FIELD:
            return "JSON_MISSING_REQUIRED_FIELD";
        case ErrorCode::JSON_INVALID_TYPE:
            return "JSON_INVALID_TYPE";
        case ErrorCode::SERIALIZATION_FAILED:
            return "SERIALIZATION_FAILED";
        case ErrorCode::DESERIALIZATION_FAILED:
            return "DESERIALIZATION_FAILED";
        case ErrorCode::BATCH_SERIALIZATION_FAILED:
            return "BATCH_SERIALIZATION_FAILED";
        case ErrorCode::BATCH_DESERIALIZATION_FAILED:
            return "BATCH_DESERIALIZATION_FAILED";
        case ErrorCode::OBJECT_CONSTRUCTION_FAILED:
            return "OBJECT_CONSTRUCTION_FAILED";
        case ErrorCode::ENUM_CONVERSION_FAILED:
            return "ENUM_CONVERSION_FAILED";
        case ErrorCode::UNSUPPORTED_TYPE:
            return "UNSUPPORTED_TYPE";

        // 适配器错误 (Adapter errors)
        case ErrorCode::ADAPTER_EXCEPTION:
            return "ADAPTER_EXCEPTION";
        case ErrorCode::ADAPTER_NOT_INITIALIZED:
            return "ADAPTER_NOT_INITIALIZED";
        case ErrorCode::ADAPTER_UNKNOWN_EXCEPTION:
            return "ADAPTER_UNKNOWN_EXCEPTION";
        case ErrorCode::NOT_IMPLEMENTED:
            return "NOT_IMPLEMENTED";
        case ErrorCode::TIMEOUT:
            return "TIMEOUT";
        case ErrorCode::MOTION_ERROR:
            return "MOTION_ERROR";

        // 网络错误 (Network errors)
        case ErrorCode::NETWORK_CONNECTION_FAILED:
            return "NETWORK_CONNECTION_FAILED";
        case ErrorCode::NETWORK_TIMEOUT:
            return "NETWORK_TIMEOUT";
        case ErrorCode::NETWORK_ERROR:
            return "NETWORK_ERROR";

        // 硬件错误 (Hardware errors)
        case ErrorCode::HARDWARE_TIMEOUT:
            return "HARDWARE_TIMEOUT";
        case ErrorCode::HARDWARE_NOT_RESPONDING:
            return "HARDWARE_NOT_RESPONDING";
        case ErrorCode::HARDWARE_COMMAND_FAILED:
            return "HARDWARE_COMMAND_FAILED";

        // 连接相关错误 (Connection errors)
        case ErrorCode::PORT_NOT_INITIALIZED:
            return "PORT_NOT_INITIALIZED";

        // 运动控制错误 (Motion control errors)
        case ErrorCode::MOTION_START_FAILED:
            return "MOTION_START_FAILED";
        case ErrorCode::MOTION_STOP_FAILED:
            return "MOTION_STOP_FAILED";
        case ErrorCode::POSITION_QUERY_FAILED:
            return "POSITION_QUERY_FAILED";
        case ErrorCode::HARDWARE_OPERATION_FAILED:
            return "HARDWARE_OPERATION_FAILED";

        // 数据持久化错误 (Data persistence errors)
        case ErrorCode::REPOSITORY_NOT_AVAILABLE:
            return "REPOSITORY_NOT_AVAILABLE";
        case ErrorCode::DATABASE_WRITE_FAILED:
            return "DATABASE_WRITE_FAILED";
        case ErrorCode::DATA_SERIALIZATION_FAILED:
            return "DATA_SERIALIZATION_FAILED";
        case ErrorCode::NOT_FOUND:
            return "NOT_FOUND";
        case ErrorCode::RECIPE_NOT_FOUND:
            return "RECIPE_NOT_FOUND";
        case ErrorCode::RECIPE_VERSION_NOT_FOUND:
            return "RECIPE_VERSION_NOT_FOUND";
        case ErrorCode::RECIPE_ALREADY_EXISTS:
            return "RECIPE_ALREADY_EXISTS";
        case ErrorCode::RECIPE_INVALID_STATE:
            return "RECIPE_INVALID_STATE";
        case ErrorCode::RECIPE_VALIDATION_FAILED:
            return "RECIPE_VALIDATION_FAILED";
        case ErrorCode::RECIPE_IMPORT_CONFLICT:
            return "RECIPE_IMPORT_CONFLICT";
        case ErrorCode::TEMPLATE_NOT_FOUND:
            return "TEMPLATE_NOT_FOUND";
        case ErrorCode::PARAMETER_SCHEMA_NOT_FOUND:
            return "PARAMETER_SCHEMA_NOT_FOUND";

        // 文件解析错误 (File parsing errors)
        case ErrorCode::FILE_PARSING_FAILED:
            return "FILE_PARSING_FAILED";
        case ErrorCode::PATH_OPTIMIZATION_FAILED:
            return "PATH_OPTIMIZATION_FAILED";
        case ErrorCode::FILE_VALIDATION_FAILED:
            return "FILE_VALIDATION_FAILED";

        // 线程操作错误 (Thread operation errors)
        case ErrorCode::THREAD_START_FAILED:
            return "THREAD_START_FAILED";

        // 兜底错误 (Fallback errors)
        case ErrorCode::UNKNOWN_ERROR:
            return "UNKNOWN_ERROR";

        default:
            return "UNKNOWN_ERROR_CODE";
    }
}

}  // namespace Types
}  // namespace Shared
}  // namespace Siligen
