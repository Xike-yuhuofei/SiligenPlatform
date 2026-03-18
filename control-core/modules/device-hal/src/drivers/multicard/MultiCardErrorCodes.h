#pragma once
#include <string>
#include <unordered_map>

namespace Siligen {
namespace Infrastructure {
namespace Hardware {

/// @brief MultiCard 错误码语义信息
struct ErrorInfo {
    int code;                 ///< 错误码
    std::string name;         ///< 错误名称（英文）
    std::string description;  ///< 错误描述（中文）
    std::string hint;         ///< 排查建议（中文）
    std::string severity;     ///< 严重程度：INFO/WARNING/ERROR/CRITICAL
};

/// @brief MultiCard API 错误码映射表
/// @details 提供错误码到语义信息的映射，帮助快速定位和解决硬件问题
/// @see MultiCard API Documentation (08_比较输出函数)
class MultiCardErrorCodes {
   private:
    static std::unordered_map<int, ErrorInfo> error_map_;
    static bool initialized_;

    /// @brief 初始化错误码映射表
    static void InitializeErrorMap();

   public:
    /// @brief 获取错误码对应的语义信息
    /// @param error_code MultiCard API 返回的错误码
    /// @return 错误信息结构体，如果错误码未知则返回 "UnknownError"
    static const ErrorInfo& GetErrorInfo(int error_code);

    /// @brief 格式化错误消息
    /// @param error_code MultiCard API 返回的错误码
    /// @param context 上下文信息（如函数名、操作描述等）
    /// @return 格式化的错误消息字符串
    /// @example
    /// FormatErrorMessage(7, "MC_GetSts")
    /// 返回: "[ParameterError] 错误码:7 参数错误 | 建议: 检测参数是否合理 | 上下文: MC_GetSts"
    static std::string FormatErrorMessage(int error_code, const std::string& context = "");

    /// @brief 检查错误码是否表示成功
    /// @param error_code MultiCard API 返回码
    /// @return true 表示成功，false 表示失败
    static bool IsSuccess(int error_code) {
        return error_code == 0;
    }

    /// @brief 检查错误码是否为通信错误
    /// @param error_code MultiCard API 返回码
    /// @return true 表示通信错误，false 表示其他错误或成功
    static bool IsCommunicationError(int error_code);

    /// @brief 检查错误码是否为参数错误
    /// @param error_code MultiCard API 返回码
    /// @return true 表示参数错误，false 表示其他错误或成功
    static bool IsParameterError(int error_code);
};

}  // namespace Hardware
}  // namespace Infrastructure
}  // namespace Siligen
