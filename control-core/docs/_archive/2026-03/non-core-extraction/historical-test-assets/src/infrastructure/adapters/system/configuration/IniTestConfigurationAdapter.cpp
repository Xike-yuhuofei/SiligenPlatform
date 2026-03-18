#include "IniTestConfigurationAdapter.h"

#include "shared/types/Error.h"
#include "shared/types/AxisTypes.h"
#include "shared/types/Types.h"

#include <fstream>
#include <sstream>
#include <windows.h>

namespace Siligen::Infrastructure::Configs {

using namespace Shared::Types;

namespace {
constexpr int kAxisCount = 2;
}

IniTestConfigurationAdapter::IniTestConfigurationAdapter() {
    m_config = loadDefaultConfig();
}

Result<TestConfiguration> IniTestConfigurationAdapter::loadConfig(const std::string& configFilePath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!configFileExists(configFilePath)) {
        return Result<TestConfiguration>::Failure(
            Error(ErrorCode::CONFIGURATION_ERROR, "Config file not found: " + configFilePath, "IniTestConfigurationAdapter"));
    }

    TestConfiguration config;

    // 加载安全限制
    config.safetyLimits.maxJogSpeed = readDouble(configFilePath, "SafetyLimits", "MaxJogSpeed", 100.0);
    config.safetyLimits.maxHomingSpeed = readDouble(configFilePath, "SafetyLimits", "MaxHomingSpeed", 50.0);

    // 加载轴限位
    for (int axis = 0; axis < kAxisCount; ++axis) {
        const auto axis_id = FromIndex(static_cast<int16>(axis));
        if (!IsValid(axis_id)) {
            continue;
        }

        std::string minKey = "Axis" + std::to_string(axis) + "_MinPosition";
        std::string maxKey = "Axis" + std::to_string(axis) + "_MaxPosition";

        double minPos = readDouble(configFilePath, "SafetyLimits", minKey, -200.0);
        double maxPos = readDouble(configFilePath, "SafetyLimits", maxKey, 200.0);

        config.safetyLimits.axisLimits[axis_id] = AxisLimits(minPos, maxPos);
    }

    // 加载测试参数
    config.testParams.homingTimeoutMs = readInt(configFilePath, "TestParameters", "HomingTimeoutMs", 30000);
    config.testParams.triggerToleranceMm = readDouble(configFilePath, "TestParameters", "TriggerToleranceMm", 0.1);
    config.testParams.cmpSampleRateHz = readInt(configFilePath, "TestParameters", "CMPSampleRateHz", 100);
    config.testParams.dataRecordEnabled = readBool(configFilePath, "TestParameters", "DataRecordEnabled", true);
    config.testParams.maxStoredRecords = readInt(configFilePath, "TestParameters", "MaxStoredRecords", 10000);

    // 加载显示参数
    config.displayParams.positionRefreshIntervalMs =
        readInt(configFilePath, "DisplayParameters", "PositionRefreshIntervalMs", 100);
    config.displayParams.chartMaxPoints = readInt(configFilePath, "DisplayParameters", "ChartMaxPoints", 1000);
    config.displayParams.showDetailedErrors = readBool(configFilePath, "DisplayParameters", "ShowDetailedErrors", true);

    // 验证配置
    auto validateResult = validateConfig(config);
    if (validateResult.IsError()) {
        return Result<TestConfiguration>::Failure(validateResult.GetError());
    }

    m_config = config;
    return Result<TestConfiguration>::Success(config);
}

Result<void> IniTestConfigurationAdapter::saveConfig(const TestConfiguration& config, const std::string& configFilePath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // 验证配置
    auto validateResult = validateConfig(config);
    if (validateResult.IsError()) {
        return validateResult;
    }

    // 保存安全限制
    writeDouble(configFilePath, "SafetyLimits", "MaxJogSpeed", config.safetyLimits.maxJogSpeed);
    writeDouble(configFilePath, "SafetyLimits", "MaxHomingSpeed", config.safetyLimits.maxHomingSpeed);

    // 保存轴限位
    for (const auto& [axis_id, limits] : config.safetyLimits.axisLimits) {
        if (!IsValid(axis_id)) {
            continue;
        }

        const int axis_index = static_cast<int>(ToIndex(axis_id));
        std::string minKey = "Axis" + std::to_string(axis_index) + "_MinPosition";
        std::string maxKey = "Axis" + std::to_string(axis_index) + "_MaxPosition";

        writeDouble(configFilePath, "SafetyLimits", minKey, limits.minPosition);
        writeDouble(configFilePath, "SafetyLimits", maxKey, limits.maxPosition);
    }

    // 保存测试参数
    writeInt(configFilePath, "TestParameters", "HomingTimeoutMs", config.testParams.homingTimeoutMs);
    writeDouble(configFilePath, "TestParameters", "TriggerToleranceMm", config.testParams.triggerToleranceMm);
    writeInt(configFilePath, "TestParameters", "CMPSampleRateHz", config.testParams.cmpSampleRateHz);
    writeBool(configFilePath, "TestParameters", "DataRecordEnabled", config.testParams.dataRecordEnabled);
    writeInt(configFilePath, "TestParameters", "MaxStoredRecords", config.testParams.maxStoredRecords);

    // 保存显示参数
    writeInt(configFilePath,
             "DisplayParameters",
             "PositionRefreshIntervalMs",
             config.displayParams.positionRefreshIntervalMs);
    writeInt(configFilePath, "DisplayParameters", "ChartMaxPoints", config.displayParams.chartMaxPoints);
    writeBool(configFilePath, "DisplayParameters", "ShowDetailedErrors", config.displayParams.showDetailedErrors);

    m_config = config;
    return Result<void>::Success();
}

TestConfiguration IniTestConfigurationAdapter::loadDefaultConfig() const {
    return TestConfiguration();  // 使用默认构造函数的默认值
}

Result<void> IniTestConfigurationAdapter::validateConfig(const TestConfiguration& config) const {
    if (!config.isValid()) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Invalid configuration", "IniTestConfigurationAdapter"));
    }
    return Result<void>::Success();
}

bool IniTestConfigurationAdapter::configFileExists(const std::string& configFilePath) const {
    std::ifstream file(configFilePath);
    return file.good();
}

SafetyLimits IniTestConfigurationAdapter::getSafetyLimits() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config.safetyLimits;
}

Result<void> IniTestConfigurationAdapter::setSafetyLimits(const SafetyLimits& limits) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.safetyLimits = limits;
    return Result<void>::Success();
}

TestParameters IniTestConfigurationAdapter::getTestParameters() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config.testParams;
}

Result<void> IniTestConfigurationAdapter::setTestParameters(const TestParameters& params) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.testParams = params;
    return Result<void>::Success();
}

DisplayParameters IniTestConfigurationAdapter::getDisplayParameters() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config.displayParams;
}

Result<void> IniTestConfigurationAdapter::setDisplayParameters(const DisplayParameters& params) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.displayParams = params;
    return Result<void>::Success();
}

Result<AxisLimits> IniTestConfigurationAdapter::getAxisLimits(LogicalAxisId axis_id) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!IsValid(axis_id)) {
        return Result<AxisLimits>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Invalid axis number", "IniTestConfigurationAdapter"));
    }

    const AxisLimits* limits = m_config.getAxisLimits(axis_id);
    if (limits == nullptr) {
        return Result<AxisLimits>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Invalid axis number", "IniTestConfigurationAdapter"));
    }

    return Result<AxisLimits>::Success(*limits);
}

Result<void> IniTestConfigurationAdapter::setAxisLimits(LogicalAxisId axis_id, const AxisLimits& limits) {
    std::lock_guard<std::mutex> lock(m_mutex);

    const int axis_index = static_cast<int>(ToIndex(axis_id));
    if (!IsValid(axis_id) || axis_index < 0 || axis_index >= kAxisCount) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Invalid axis number", "IniTestConfigurationAdapter"));
    }

    if (!limits.isValid()) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Invalid axis limits", "IniTestConfigurationAdapter"));
    }

    m_config.safetyLimits.axisLimits[axis_id] = limits;
    return Result<void>::Success();
}

Result<void> IniTestConfigurationAdapter::resetToDefault() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = loadDefaultConfig();
    return Result<void>::Success();
}

Result<void> IniTestConfigurationAdapter::createDefaultConfigFile(const std::string& configFilePath) {
    TestConfiguration defaultConfig = loadDefaultConfig();
    return saveConfig(defaultConfig, configFilePath);
}

// INI辅助方法实现
double IniTestConfigurationAdapter::readDouble(const std::string& filePath,
                                        const std::string& section,
                                        const std::string& key,
                                        double defaultValue) const {
    char buffer[256];
    GetPrivateProfileStringA(section.c_str(), key.c_str(), "", buffer, sizeof(buffer), filePath.c_str());

    if (strlen(buffer) == 0) {
        return defaultValue;
    }

    return std::stod(buffer);
}

int IniTestConfigurationAdapter::readInt(const std::string& filePath,
                                  const std::string& section,
                                  const std::string& key,
                                  int defaultValue) const {
    return GetPrivateProfileIntA(section.c_str(), key.c_str(), defaultValue, filePath.c_str());
}

bool IniTestConfigurationAdapter::readBool(const std::string& filePath,
                                    const std::string& section,
                                    const std::string& key,
                                    bool defaultValue) const {
    int intValue = readInt(filePath, section, key, defaultValue ? 1 : 0);
    return intValue != 0;
}

Result<void> IniTestConfigurationAdapter::writeDouble(const std::string& filePath,
                                               const std::string& section,
                                               const std::string& key,
                                               double value) const {
    std::ostringstream oss;
    oss << value;

    BOOL result = WritePrivateProfileStringA(section.c_str(), key.c_str(), oss.str().c_str(), filePath.c_str());
    if (!result) {
        return Result<void>::Failure(
            Error(ErrorCode::CONFIGURATION_ERROR, "Failed to write config value", "IniTestConfigurationAdapter"));
    }

    return Result<void>::Success();
}

Result<void> IniTestConfigurationAdapter::writeInt(const std::string& filePath,
                                            const std::string& section,
                                            const std::string& key,
                                            int value) const {
    std::ostringstream oss;
    oss << value;

    BOOL result = WritePrivateProfileStringA(section.c_str(), key.c_str(), oss.str().c_str(), filePath.c_str());
    if (!result) {
        return Result<void>::Failure(
            Error(ErrorCode::CONFIGURATION_ERROR, "Failed to write config value", "IniTestConfigurationAdapter"));
    }

    return Result<void>::Success();
}

Result<void> IniTestConfigurationAdapter::writeBool(const std::string& filePath,
                                             const std::string& section,
                                             const std::string& key,
                                             bool value) const {
    return writeInt(filePath, section, key, value ? 1 : 0);
}

}  // namespace Siligen::Infrastructure::Configs

