#include "siligen/device/adapters/drivers/multicard/error_mapper.h"

#include <mutex>
#include <sstream>

namespace Siligen::Hal::Drivers::MultiCard {

std::unordered_map<int, ErrorInfo> ErrorMapper::error_map_;
bool ErrorMapper::initialized_ = false;

void ErrorMapper::InitializeErrorMap() {
    if (initialized_) {
        return;
    }

    error_map_[0] = {0, "Success", "操作成功", "", "INFO"};
    error_map_[1] = {1, "ExecutionFailed", "执行失败", "检测命令执行条件是否满足", "ERROR"};
    error_map_[2] = {2, "ApiNotSupported", "版本不支持该API", "如有需要，联系厂家", "WARNING"};
    error_map_[7] = {7, "ParameterError", "参数错误", "检测参数是否合理", "WARNING"};
    error_map_[-1] = {-1, "CommunicationFailed", "通讯失败", "检查接线是否牢靠，必要时更换板卡", "CRITICAL"};
    error_map_[-6] = {-6, "CardOpenFailed", "打开控制器失败", "检查串口名/IP与端口配置，避免重复调用 MC_Open", "ERROR"};
    error_map_[-7] = {-7, "ControllerNoResponse", "运动控制器无响应", "检测控制器是否连接、是否打开，必要时更换板卡", "CRITICAL"};
    error_map_[-8] = {-8, "ComOpenFailed", "串口打开失败", "检查串口权限、占用或驱动", "ERROR"};

    initialized_ = true;
}

const ErrorInfo& ErrorMapper::GetErrorInfo(int error_code) {
    static std::once_flag init_flag;
    std::call_once(init_flag, []() { InitializeErrorMap(); });

    auto it = error_map_.find(error_code);
    if (it != error_map_.end()) {
        return it->second;
    }

    static thread_local ErrorInfo unknown_error;
    unknown_error.code = error_code;
    unknown_error.name = "UnknownError";
    unknown_error.description = "未知错误码";
    unknown_error.hint = "请联系技术支持并提供错误码: " + std::to_string(error_code);
    unknown_error.severity = "WARNING";
    return unknown_error;
}

std::string ErrorMapper::FormatErrorMessage(int error_code, const std::string& context) {
    const ErrorInfo& info = GetErrorInfo(error_code);
    std::ostringstream oss;
    oss << "[" << info.name << "] 错误码:" << error_code << " " << info.description;
    if (!info.hint.empty()) {
        oss << " | 建议: " << info.hint;
    }
    if (!context.empty()) {
        oss << " | 上下文: " << context;
    }
    return oss.str();
}

bool ErrorMapper::IsCommunicationError(int error_code) {
    return error_code == -1 || error_code == -6 || error_code == -7 || error_code == -8;
}

bool ErrorMapper::IsParameterError(int error_code) {
    return error_code == 7;
}

}  // namespace Siligen::Hal::Drivers::MultiCard
