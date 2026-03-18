/**
 * @file ErrorHandler.h
 * @brief 统一错误处理系统头文件
 * @details 提供完整的错误码定义、异常处理和错误报告功能
 *
 * @author Claude Code
 * @date 2025-10-30
 */

#pragma once

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#ifdef ERROR
#undef ERROR
#endif

#ifdef SUCCESS
#undef SUCCESS
#endif

namespace Siligen {
namespace Shared {

/**
 * @brief 系统错误码枚举
 */
enum class SystemErrorCode : int32_t {
    // 通用错误码
    SUCCESS = 0,
    UNKNOWN_ERROR = -1,

    // 连接相关错误 (-100 to -199)
    CARD_CONNECTION_FAILED = -100,
    NETWORK_TIMEOUT = -101,
    INVALID_IP_ADDRESS = -102,
    PORT_ACCESS_DENIED = -103,

    // 硬件初始化错误 (-200 to -299)
    AXIS_ENABLE_FAILED = -200,
    COORDINATE_SYSTEM_FAILED = -201,
    SERVO_ALARM_ACTIVE = -202,
    LIMIT_SWITCH_TRIGGERED = -203,

    // 运动控制错误 (-300 to -399)
    MOTION_COMMAND_FAILED = -300,
    POSITION_FOLLOW_ERROR = -301,
    VELOCITY_LIMIT_EXCEEDED = -302,
    ACCELERATION_LIMIT_EXCEEDED = -303,
    MOTION_TIMEOUT = -304,

    // CMP触发错误 (-400 to -499)
    CMP_SETUP_FAILED = -400,
    CMP_TRIGGER_FAILED = -401,
    INVALID_PULSE_WIDTH = -402,
    CMP_CHANNEL_ERROR = -403,

    // 参数错误 (-500 to -599)
    INVALID_PARAMETERS = -500,
    PARAMETER_OUT_OF_RANGE = -501,
    MISSING_REQUIRED_PARAMETER = -502,
    PARAMETER_CONFLICT = -503,

    // 系统错误 (-600 to -699)
    MEMORY_ALLOCATION_FAILED = -600,
    THREAD_CREATION_FAILED = -601,
    FILE_ACCESS_FAILED = -602,
    CONFIGURATION_ERROR = -603,

    // 安全相关错误 (-700 to -799)
    EMERGENCY_STOP_ACTIVATED = -700,
    SAFETY_CHECK_FAILED = -701,
    HARDWARE_FAULT_DETECTED = -702,
    COMMUNICATION_LOST = -703,

    // 安全模块认证错误 (-710 to -729)
    AUTHENTICATION_FAILED = -710,      // 认证失败
    INVALID_USERNAME_PASSWORD = -711,  // 用户名或密码错误
    ACCOUNT_LOCKED = -712,             // 账号已锁定
    SESSION_EXPIRED = -713,            // 会话超时
    SESSION_INVALID = -714,            // 会话无效
    PASSWORD_WEAK = -715,              // 密码强度不足
    USER_NOT_FOUND = -716,             // 用户不存在
    USER_ALREADY_EXISTS = -717,        // 用户已存在

    // 安全模块权限错误 (-730 to -749)
    PERMISSION_DENIED = -730,    // 权限不足
    ROLE_INVALID = -731,         // 角色无效
    UNAUTHORIZED_ACCESS = -732,  // 未授权访问

    // 安全限制错误 (-750 to -769)
    MOTION_SPEED_EXCEEDED = -750,         // 运动速度超限
    MOTION_ACCELERATION_EXCEEDED = -751,  // 运动加速度超限
    MOTION_JERK_EXCEEDED = -752,          // 运动加加速度超限
    WORKSPACE_LIMIT_EXCEEDED = -753,      // 工作空间位置超限
    SAFETY_BOUNDARY_VIOLATED = -754,      // 违反安全边界
    MOTION_PARAMETER_INVALID = -755,      // 运动参数无效

    // 网络安全错误 (-770 to -779)
    IP_NOT_WHITELISTED = -770,        // IP未在白名单中
    CONNECTION_BLOCKED = -771,        // 连接被阻止
    TOO_MANY_FAILED_ATTEMPTS = -772,  // 连接尝试次数过多

    // 审计日志错误 (-780 to -789)
    AUDIT_LOG_WRITE_FAILED = -780,  // 审计日志写入失败
    AUDIT_LOG_READ_FAILED = -781,   // 审计日志读取失败
    AUDIT_LOG_STORAGE_FULL = -782,  // 审计日志存储空间不足

    // 配置错误 (-790 to -799)
    CONFIG_VERSION_INCOMPATIBLE = -790,  // 配置版本不兼容
    CONFIG_BACKUP_FAILED = -791,         // 配置备份失败
    CONFIG_LOAD_FAILED = -792,           // 配置加载失败
    CONFIG_VALIDATION_FAILED = -793      // 配置验证失败
};

/**
 * @brief 硬件特定错误码
 */
enum class HardwareErrorCode : int32_t {
    // MultiCard API错误映射
    COMMUNICATION_FAILED = -1,
    CARD_OPEN_FAIL = -6,
    EXECUTION_FAILED = 1,
    API_NOT_SUPPORTED = 2,
    PARAMETER_ERROR = 7,
    CONTROLLER_NO_RESPONSE = -7,
    COM_OPEN_FAILED = -8,

    // 伺服驱动器错误 (1000-1099)
    SERVO_OVERCURRENT = 1001,
    SERVO_OVERVOLTAGE = 1002,
    SERVO_UNDERVOLTAGE = 1003,
    ENCODER_FAILURE = 1004,
    MOTOR_OVERHEAT = 1005,
    SERVO_PARAMETER_ERROR = 1006,
    POSITION_ERROR_EXCEEDED = 1007,
    FOLLOWING_ERROR_EXCEEDED = 1008,

    // 限位开关错误 (1100-1199)
    POSITIVE_HARD_LIMIT = 1101,
    NEGATIVE_HARD_LIMIT = 1102,
    POSITIVE_SOFT_LIMIT = 1103,
    NEGATIVE_SOFT_LIMIT = 1104,
    LIMIT_SWITCH_FAULT = 1105,

    // 通信错误 (1200-1299)
    ETHERNET_COMMUNICATION_FAILED = 1201,
    PROTOCOL_ERROR = 1202,
    DATA_CORRUPTION = 1203,
    COMMUNICATION_TIMEOUT = 1204,
    DEVICE_NOT_RESPONDING = 1205,

    // 运动控制错误 (1300-1399)
    INTERPOLATION_FAILED = 1301,
    PATH_PLANNING_ERROR = 1302,
    VELOCITY_PROFILE_ERROR = 1303,
    ACCELERATION_PROFILE_ERROR = 1304,
    JERK_LIMIT_EXCEEDED = 1305,
    PATH_CONTINUITY_ERROR = 1306,

    // CMP/触发错误 (1400-1499)
    CMP_BUFFER_OVERFLOW = 1401,
    CMP_TIMING_ERROR = 1402,
    TRIGGER_SIGNAL_LOST = 1403,
    PULSE_GENERATION_FAILED = 1404,
    TRIGGER_CONFIGURATION_ERROR = 1405
};

/**
 * @brief 错误严重性级别
 */
enum class ErrorSeverity {
    INFO,     // 信息
    WARNING,  // 警告
    ERROR,    // 错误
    CRITICAL  // 严重错误
};

// ==================== 安全模块类型定义 ====================

/**
 * @brief 审计类别
 */
enum class AuditCategory {
    AUTHENTICATION,   // 认证相关
    AUTHORIZATION,    // 授权相关
    MOTION_CONTROL,   // 运动控制
    PARAMETER_CHANGE, // 参数修改
    SYSTEM_CONFIG,    // 系统配置
    SAFETY_EVENT,     // 安全相关
    NETWORK_ACCESS,   // 网络访问
    USER_MANAGEMENT,  // 用户管理
    DATA_ACCESS,      // 数据访问

    // 兼容旧命名
    SAFETY = SAFETY_EVENT
};

/**
 * @brief 审计级别
 */
enum class AuditLevel {
    INFO = 0,     // 信息
    WARNING = 1,  // 警告
    ERROR = 2,    // 错误
    SECURITY = 3, // 安全事件

    // 兼容旧命名
    LOW = INFO,
    MEDIUM = WARNING,
    HIGH = ERROR,
    CRITICAL = SECURITY
};

/**
 * @brief 审计状态
 */
enum class AuditStatus {
    SUCCESS = 0,  // 成功
    FAILURE = 1,  // 失败
    DENIED = 2,   // 拒绝
    PENDING = 3,  // 待处理
    CANCELLED = 4,// 已取消

    // 兼容旧命名
    FAILED = FAILURE
};

/**
 * @brief 权限类型
 */
enum class Permission {
    VIEW_STATUS = 1 << 0,          // 查看状态
    JOG_MOTION = 1 << 1,           // 点动/手动
    HOMING = 1 << 2,               // 回零
    DISPENSING = 1 << 3,           // 点胶
    CONFIGURE_PARAMETERS = 1 << 4, // 配置参数
    MANAGE_TASKS = 1 << 5,         // 任务管理
    LOAD_FILES = 1 << 6,           // 加载文件
    SYSTEM_DIAGNOSTICS = 1 << 7,   // 系统诊断
    CALIBRATION = 1 << 8,          // 校准
    VIEW_AUDIT_LOG = 1 << 9,       // 查看审计日志
    USER_MANAGEMENT = 1 << 10,     // 用户管理
    SYSTEM_CONFIG = 1 << 11,       // 系统配置
    SECURITY_CONFIG = 1 << 12,     // 安全配置
    EXPORT_DATA = 1 << 13,         // 导出数据
    ALL = (1 << 14) - 1            // 所有权限
};

/**
 * @brief 用户角色
 */
enum class UserRole {
    GUEST = 0,       // 访客
    OPERATOR = 1,    // 操作员
    SUPERVISOR = 2,  // 监督员
    ADMIN = 3,       // 管理员
    SUPER_ADMIN = 4, // 超级管理员
    MAINTAINER = 5,  // 维护员

    // 兼容旧命名
    ADMINISTRATOR = ADMIN
};

/**
 * @brief 支持位运算的Permission运算符
 */
inline Permission operator|(Permission a, Permission b) {
    return static_cast<Permission>(static_cast<int>(a) | static_cast<int>(b));
}

inline Permission& operator|=(Permission& a, Permission b) {
    a = a | b;
    return a;
}

inline Permission operator&(Permission a, Permission b) {
    return static_cast<Permission>(static_cast<int>(a) & static_cast<int>(b));
}

inline bool HasPermission(Permission userPerms, Permission required) {
    return (userPerms & required) == required;
}

/**
 * @brief 错误信息结构
 */
struct ErrorInfo {
    SystemErrorCode system_code;
    HardwareErrorCode hardware_code;
    ErrorSeverity severity;
    std::string message;
    std::string context;
    std::string function;
    int line;
    std::string file;

    ErrorInfo()
        : system_code(SystemErrorCode::SUCCESS),
          hardware_code(HardwareErrorCode::CARD_OPEN_FAIL),
          severity(ErrorSeverity::INFO),
          line(0) {}

    bool IsSuccess() const {
        return system_code == SystemErrorCode::SUCCESS;
    }

    bool IsError() const {
        return system_code != SystemErrorCode::SUCCESS;
    }

    std::string GetFullMessage() const;
};

/**
 * @brief 统一错误处理器类
 */
class ErrorHandler {
   public:
    /**
     * @brief 获取单例实例
     */
    static ErrorHandler& GetInstance();

    /**
     * @brief 处理成功结果
     */
    static ErrorInfo Success();

    /**
     * @brief 创建系统错误
     */
    static ErrorInfo SystemError(SystemErrorCode code,
                                 const std::string& message,
                                 const std::string& context = "",
                                 const std::string& function = "",
                                 int line = 0,
                                 const std::string& file = "");

    /**
     * @brief 创建硬件错误
     */
    static ErrorInfo HardwareError(HardwareErrorCode code,
                                   const std::string& message,
                                   const std::string& context = "",
                                   const std::string& function = "",
                                   int line = 0,
                                   const std::string& file = "");

    /**
     * @brief 处理通用错误（零依赖版本）
     */
    static ErrorInfo HandleGenericError(int error_code, const std::string& operation, const std::string& context = "");

    /**
     * @brief 报告错误
     */
    static void ReportError(const ErrorInfo& error);

    /**
     * @brief 获取错误描述
     */
    static std::string GetErrorCodeDescription(SystemErrorCode code);
    static std::string GetErrorCodeDescription(HardwareErrorCode code);

    /**
     * @brief 设置是否在错误时退出程序
     */
    static void SetExitOnError(bool exit_on_error);

    /**
     * @brief 设置详细输出模式
     */
    static void SetVerboseMode(bool verbose);

   public:
    static bool s_exit_on_error;
    static bool s_verbose_mode;

    ErrorHandler() = default;
    ~ErrorHandler() = default;
    ErrorHandler(const ErrorHandler&) = delete;
    ErrorHandler& operator=(const ErrorHandler&) = delete;

    static ErrorSeverity DetermineSeverity(SystemErrorCode system_code, HardwareErrorCode hardware_code);
    static void FormatErrorOutput(const ErrorInfo& error);
};

/**
 * @brief 错误处理宏定义
 */
#define REPORT_SUCCESS() ErrorHandler::Success()

#define REPORT_SYSTEM_ERROR(code, message) \
    ErrorHandler::SystemError(code, message, "", __FUNCTION__, __LINE__, __FILE__)

#define REPORT_SYSTEM_ERROR_CTX(code, message, context) \
    ErrorHandler::SystemError(code, message, context, __FUNCTION__, __LINE__, __FILE__)

#define REPORT_HARDWARE_ERROR(code, message) \
    ErrorHandler::HardwareError(code, message, "", __FUNCTION__, __LINE__, __FILE__)

#define REPORT_HARDWARE_ERROR_CTX(code, message, context) \
    ErrorHandler::HardwareError(code, message, context, __FUNCTION__, __LINE__, __FILE__)

#define REPORT_GENERIC_ERROR(result, operation) ErrorHandler::HandleGenericError(result, operation, "")

#define REPORT_GENERIC_ERROR_CTX(result, operation, context) \
    ErrorHandler::HandleGenericError(result, operation, context)

/**
 * @brief 错误检查和处理宏
 */
#define CHECK_SYSTEM_ERROR(condition, code, message)           \
    do {                                                       \
        if (!(condition)) {                                    \
            auto error = REPORT_SYSTEM_ERROR(code, message);   \
            ErrorHandler::ReportError(error);                  \
            if (ErrorHandler::GetInstance().IsExitOnError()) { \
                exit(static_cast<int>(code));                  \
            }                                                  \
            return error;                                      \
        }                                                      \
    } while (0)

#define CHECK_HARDWARE_ERROR(result, operation)                \
    do {                                                       \
        auto error = REPORT_GENERIC_ERROR(result, operation);  \
        if (error.IsError()) {                                 \
            ErrorHandler::ReportError(error);                  \
            if (ErrorHandler::GetInstance().IsExitOnError()) { \
                exit(static_cast<int>(error.system_code));     \
            }                                                  \
            return error;                                      \
        }                                                      \
    } while (0)

#define CHECK_SYSTEM_ERROR_RETURN(condition, code, message)  \
    do {                                                     \
        if (!(condition)) {                                  \
            auto error = REPORT_SYSTEM_ERROR(code, message); \
            ErrorHandler::ReportError(error);                \
            return error;                                    \
        }                                                    \
    } while (0)

}  // namespace Shared

// Compatibility aliases for legacy code that still expects the security/error
// enums in the top-level Siligen namespace.
using Shared::AuditCategory;
using Shared::AuditLevel;
using Shared::AuditStatus;
using Shared::ErrorInfo;
using Shared::ErrorSeverity;
using Shared::HardwareErrorCode;
using Shared::Permission;
using Shared::SystemErrorCode;
using Shared::UserRole;

}  // namespace Siligen
