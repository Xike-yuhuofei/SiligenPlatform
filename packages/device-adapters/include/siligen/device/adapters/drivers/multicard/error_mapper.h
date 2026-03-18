#pragma once

#include <string>
#include <unordered_map>

namespace Siligen::Hal::Drivers::MultiCard {

struct ErrorInfo {
    int code = 0;
    std::string name;
    std::string description;
    std::string hint;
    std::string severity;
};

class ErrorMapper {
   public:
    static const ErrorInfo& GetErrorInfo(int error_code);
    static std::string FormatErrorMessage(int error_code, const std::string& context = {});
    static bool IsSuccess(int error_code) { return error_code == 0; }
    static bool IsCommunicationError(int error_code);
    static bool IsParameterError(int error_code);

   private:
    static void InitializeErrorMap();
    static std::unordered_map<int, ErrorInfo> error_map_;
    static bool initialized_;
};

}  // namespace Siligen::Hal::Drivers::MultiCard
