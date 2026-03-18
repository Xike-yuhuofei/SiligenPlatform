#include "MultiCardErrorCodes.h"

#include "siligen/hal/drivers/multicard/error_mapper.h"

namespace Siligen {
namespace Infrastructure {
namespace Hardware {

std::unordered_map<int, ErrorInfo> MultiCardErrorCodes::error_map_;
bool MultiCardErrorCodes::initialized_ = false;

void MultiCardErrorCodes::InitializeErrorMap() {
    initialized_ = true;
}

const ErrorInfo& MultiCardErrorCodes::GetErrorInfo(int error_code) {
    const auto& mapped = Siligen::Hal::Drivers::MultiCard::ErrorMapper::GetErrorInfo(error_code);
    static thread_local ErrorInfo converted;
    converted.code = mapped.code;
    converted.name = mapped.name;
    converted.description = mapped.description;
    converted.hint = mapped.hint;
    converted.severity = mapped.severity;
    return converted;
}

std::string MultiCardErrorCodes::FormatErrorMessage(int error_code, const std::string& context) {
    return Siligen::Hal::Drivers::MultiCard::ErrorMapper::FormatErrorMessage(error_code, context);
}

bool MultiCardErrorCodes::IsCommunicationError(int error_code) {
    return Siligen::Hal::Drivers::MultiCard::ErrorMapper::IsCommunicationError(error_code);
}

bool MultiCardErrorCodes::IsParameterError(int error_code) {
    return Siligen::Hal::Drivers::MultiCard::ErrorMapper::IsParameterError(error_code);
}

}  // namespace Hardware
}  // namespace Infrastructure
}  // namespace Siligen
