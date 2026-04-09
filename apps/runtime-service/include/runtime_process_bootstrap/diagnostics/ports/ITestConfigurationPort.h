#pragma once

#include "runtime_process_bootstrap/diagnostics/aggregates/TestConfiguration.h"
#include "shared/types/Result.h"

#include <string>

namespace Siligen::Domain::Diagnostics::Ports {

using Shared::Types::Result;
using Shared::Types::LogicalAxisId;
using Siligen::Domain::Diagnostics::Aggregates::AxisLimits;
using Siligen::Domain::Diagnostics::Aggregates::DisplayParameters;
using Siligen::Domain::Diagnostics::Aggregates::SafetyLimits;
using Siligen::Domain::Diagnostics::Aggregates::TestConfiguration;
using Siligen::Domain::Diagnostics::Aggregates::TestParameters;

class ITestConfigurationPort {
   public:
    virtual ~ITestConfigurationPort() = default;

    virtual Result<TestConfiguration> loadConfig(const std::string& configFilePath) = 0;
    virtual Result<void> saveConfig(const TestConfiguration& config, const std::string& configFilePath) = 0;
    virtual TestConfiguration loadDefaultConfig() const = 0;

    virtual Result<void> validateConfig(const TestConfiguration& config) const = 0;
    virtual bool configFileExists(const std::string& configFilePath) const = 0;

    virtual SafetyLimits getSafetyLimits() const = 0;
    virtual Result<void> setSafetyLimits(const SafetyLimits& limits) = 0;

    virtual TestParameters getTestParameters() const = 0;
    virtual Result<void> setTestParameters(const TestParameters& params) = 0;

    virtual DisplayParameters getDisplayParameters() const = 0;
    virtual Result<void> setDisplayParameters(const DisplayParameters& params) = 0;

    virtual Result<AxisLimits> getAxisLimits(LogicalAxisId axis) const = 0;
    virtual Result<void> setAxisLimits(LogicalAxisId axis, const AxisLimits& limits) = 0;

    virtual Result<void> resetToDefault() = 0;
    virtual Result<void> createDefaultConfigFile(const std::string& configFilePath) = 0;
};

}  // namespace Siligen::Domain::Diagnostics::Ports
