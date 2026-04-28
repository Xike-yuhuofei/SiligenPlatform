/**
 * @file ErrorHandler.cpp
 * @brief 统一错误处理系统实现
 *
 * @author Claude Code
 * @date 2025-10-30
 */

#include "ErrorHandler.h"

#include "ErrorDescriptions.h"

#include <ctime>
#include <iomanip>
#include <sstream>

// 定义安全的编译选项
#define _CRT_SECURE_NO_WARNINGS

namespace Siligen {
namespace Shared {

// 静态成员初始化
bool ErrorHandler::s_exit_on_error = true;
bool ErrorHandler::s_verbose_mode = false;

std::string ErrorInfo::GetFullMessage() const {
    std::ostringstream oss;

    // 时间戳
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    oss << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] ";

    // 严重性级别
    switch (severity) {
        case ErrorSeverity::INFO:
            oss << "[INFO] ";
            break;
        case ErrorSeverity::WARNING:
            oss << "[WARNING] ";
            break;
        case ErrorSeverity::ERROR:
            oss << "[ERROR] ";
            break;
        case ErrorSeverity::CRITICAL:
            oss << "[CRITICAL] ";
            break;
    }

    // 主要错误信息
    oss << message;

    // 错误码
    if (system_code != SystemErrorCode::SUCCESS) {
        oss << " (系统错误码: " << static_cast<int>(system_code) << ")";
    }
    if (hardware_code != HardwareErrorCode::CARD_OPEN_FAIL) {
        oss << " (硬件错误码: " << static_cast<int>(hardware_code) << ")";
    }

    // 上下文信息
    if (!context.empty()) {
        oss << " | 上下文: " << context;
    }

    // 函数和位置信息（详细模式）
    if (ErrorHandler::s_verbose_mode && !function.empty()) {
        oss << " | 函数: " << function;
        if (line > 0) {
            oss << ":" << line;
        }
        if (!file.empty()) {
            size_t pos = file.find_last_of("/\\");
            std::string filename = (pos != std::string::npos) ? file.substr(pos + 1) : file;
            oss << " (" << filename << ")";
        }
    }

    return oss.str();
}

ErrorHandler& ErrorHandler::GetInstance() {
    static ErrorHandler instance;
    return instance;
}

ErrorInfo ErrorHandler::Success() {
    ErrorInfo info;
    info.system_code = SystemErrorCode::SUCCESS;
    info.hardware_code = HardwareErrorCode::CARD_OPEN_FAIL;
    info.severity = ErrorSeverity::INFO;
    info.message = "操作成功完成";
    info.context.clear();
    info.function.clear();
    info.line = 0;
    info.file.clear();
    return info;
}

ErrorInfo ErrorHandler::SystemError(SystemErrorCode code,
                                    const std::string& message,
                                    const std::string& context,
                                    const std::string& function,
                                    int line,
                                    const std::string& file) {
    ErrorInfo info;
    info.system_code = code;
    info.hardware_code = HardwareErrorCode::CARD_OPEN_FAIL;
    info.message = message;
    info.context = context;
    info.function = function;
    info.line = line;
    info.file = file;
    info.severity = DetermineSeverity(code, HardwareErrorCode::CARD_OPEN_FAIL);
    return info;
}

ErrorInfo ErrorHandler::HardwareError(HardwareErrorCode code,
                                      const std::string& message,
                                      const std::string& context,
                                      const std::string& function,
                                      int line,
                                      const std::string& file) {
    ErrorInfo info;
    info.system_code = SystemErrorCode::UNKNOWN_ERROR;
    info.hardware_code = code;
    info.message = message;
    info.context = context;
    info.function = function;
    info.line = line;
    info.file = file;
    info.severity = DetermineSeverity(SystemErrorCode::UNKNOWN_ERROR, code);
    return info;
}

// 硬件错误处理已移至基础设施层
// 共享内核ErrorHandler保持零依赖特性，仅处理通用错误类型
ErrorInfo ErrorHandler::HandleGenericError(int error_code, const std::string& operation, const std::string& context) {
    // 通用错误处理，不依赖具体硬件实现
    if (error_code == 0) {
        return Success();
    }

    HardwareErrorCode hw_code = static_cast<HardwareErrorCode>(error_code);
    std::ostringstream oss;
    oss << "操作失败: " << operation << " (错误码: " << error_code << ")";

    return HardwareError(hw_code, oss.str(), context);
}

void ErrorHandler::ReportError(const ErrorInfo& error) {
    FormatErrorOutput(error);
}

std::string ErrorHandler::GetErrorCodeDescription(SystemErrorCode code) {
    return GetSystemErrorDescription(code);
}

std::string ErrorHandler::GetErrorCodeDescription(HardwareErrorCode code) {
    return GetHardwareErrorDescription(code);
}

void ErrorHandler::SetExitOnError(bool exit_on_error) {
    s_exit_on_error = exit_on_error;
}

void ErrorHandler::SetVerboseMode(bool verbose) {
    s_verbose_mode = verbose;
}

ErrorSeverity ErrorHandler::DetermineSeverity(SystemErrorCode system_code, HardwareErrorCode hardware_code) {
    // 根据错误码确定严重性级别
    if (system_code == SystemErrorCode::SUCCESS) {
        return ErrorSeverity::INFO;
    }

    // 严重错误（需要立即停止）
    if (system_code == SystemErrorCode::EMERGENCY_STOP_ACTIVATED ||
        system_code == SystemErrorCode::HARDWARE_FAULT_DETECTED || system_code == SystemErrorCode::COMMUNICATION_LOST ||
        hardware_code == HardwareErrorCode::SERVO_OVERCURRENT ||
        hardware_code == HardwareErrorCode::SERVO_OVERVOLTAGE ||
        hardware_code == HardwareErrorCode::POSITIVE_HARD_LIMIT ||
        hardware_code == HardwareErrorCode::NEGATIVE_HARD_LIMIT) {
        return ErrorSeverity::CRITICAL;
    }

    // 一般错误
    if (system_code == SystemErrorCode::AXIS_ENABLE_FAILED ||
        system_code == SystemErrorCode::COORDINATE_SYSTEM_FAILED ||
        system_code == SystemErrorCode::MOTION_COMMAND_FAILED || system_code == SystemErrorCode::CMP_SETUP_FAILED ||
        hardware_code == HardwareErrorCode::EXECUTION_FAILED || hardware_code == HardwareErrorCode::ENCODER_FAILURE ||
        hardware_code == HardwareErrorCode::INTERPOLATION_FAILED) {
        return ErrorSeverity::ERROR;
    }

    // 警告
    return ErrorSeverity::WARNING;
}

void ErrorHandler::FormatErrorOutput(const ErrorInfo& error) {
    std::string output = error.GetFullMessage();

    // 根据严重性选择输出流和颜色
    switch (error.severity) {
        case ErrorSeverity::INFO:
        case ErrorSeverity::WARNING:
            std::cout << output << std::endl;
            break;
        case ErrorSeverity::ERROR:
            std::cerr << output << std::endl;
            break;
        case ErrorSeverity::CRITICAL:
            std::cerr << "[CRITICAL] " << output << std::endl;

            // 严重错误时显示额外的解决建议
            if (error.hardware_code == HardwareErrorCode::SERVO_OVERCURRENT) {
                std::cerr << "[SUGGESTION] 建议解决方案: 检查电机负载和机械卡死情况" << std::endl;
            } else if (error.hardware_code == HardwareErrorCode::POSITIVE_HARD_LIMIT ||
                       error.hardware_code == HardwareErrorCode::NEGATIVE_HARD_LIMIT) {
                std::cerr << "[SUGGESTION] 建议解决方案: 手动移动轴离开限位位置" << std::endl;
            } else if (error.system_code == SystemErrorCode::COORDINATE_SYSTEM_FAILED) {
                std::cerr << "[SUGGESTION] 建议解决方案: 检查轴状态和License限制" << std::endl;
            }
            break;
    }

    // 如果是错误且设置了退出模式，则退出程序
    if ((error.severity == ErrorSeverity::ERROR || error.severity == ErrorSeverity::CRITICAL) && s_exit_on_error) {
        std::cerr << "[ERROR] 程序因错误而退出" << std::endl;
        exit(static_cast<int>(error.system_code));
    }
}

}  // namespace Shared
}  // namespace Siligen
