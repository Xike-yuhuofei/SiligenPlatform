#pragma once

#include <string>

namespace Siligen::Device::Contracts::Faults {

enum class DeviceFaultSeverity {
    kInfo,
    kWarning,
    kError,
    kCritical,
};

enum class DeviceFaultCategory {
    kConnectivity,
    kMotion,
    kIo,
    kDispensing,
    kVendorSdk,
    kUnknown,
};

struct DeviceFault {
    std::string code;
    DeviceFaultSeverity severity = DeviceFaultSeverity::kError;
    DeviceFaultCategory category = DeviceFaultCategory::kUnknown;
    std::string message;
    std::string hint;
    bool active = true;
};

}  // namespace Siligen::Device::Contracts::Faults
