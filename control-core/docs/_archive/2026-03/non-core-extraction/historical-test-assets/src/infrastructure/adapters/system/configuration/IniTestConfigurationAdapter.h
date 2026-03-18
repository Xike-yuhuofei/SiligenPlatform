#pragma once

#include "domain/diagnostics/ports/ITestConfigurationPort.h"

#include <mutex>

namespace Siligen::Infrastructure::Configs {

using Domain::Diagnostics::Ports::ITestConfigurationPort;
using Domain::Diagnostics::Ports::Result;
using Domain::Diagnostics::Ports::TestConfiguration;
using Domain::Diagnostics::Ports::SafetyLimits;
using Domain::Diagnostics::Ports::TestParameters;
using Domain::Diagnostics::Ports::DisplayParameters;
using Domain::Diagnostics::Ports::AxisLimits;
using Siligen::Shared::Types::LogicalAxisId;

/**
 * @brief INI文件测试配置管理实现
 *
 * 使用Windows INI API读写配置文件。
 * 线程安全: 使用互斥锁保护配置访问。
 */
class IniTestConfigurationAdapter : public ITestConfigurationPort {
   public:
    /**
     * @brief 构造函数
     */
    IniTestConfigurationAdapter();

    /**
     * @brief 析构函数
     */
    ~IniTestConfigurationAdapter() override = default;

    // ============ ITestConfigurationPort接口实现 ============

    Result<TestConfiguration> loadConfig(const std::string& configFilePath) override;
    Result<void> saveConfig(const TestConfiguration& config, const std::string& configFilePath) override;
    TestConfiguration loadDefaultConfig() const override;

    Result<void> validateConfig(const TestConfiguration& config) const override;
    bool configFileExists(const std::string& configFilePath) const override;

    SafetyLimits getSafetyLimits() const override;
    Result<void> setSafetyLimits(const SafetyLimits& limits) override;

    TestParameters getTestParameters() const override;
    Result<void> setTestParameters(const TestParameters& params) override;

    DisplayParameters getDisplayParameters() const override;
    Result<void> setDisplayParameters(const DisplayParameters& params) override;

    Result<AxisLimits> getAxisLimits(LogicalAxisId axis) const override;
    Result<void> setAxisLimits(LogicalAxisId axis, const AxisLimits& limits) override;

    Result<void> resetToDefault() override;
    Result<void> createDefaultConfigFile(const std::string& configFilePath) override;

   private:
    TestConfiguration m_config;
    mutable std::mutex m_mutex;

    /**
     * @brief 从INI文件读取double值
     */
    double readDouble(const std::string& filePath,
                      const std::string& section,
                      const std::string& key,
                      double defaultValue) const;

    /**
     * @brief 从INI文件读取int值
     */
    int readInt(const std::string& filePath,
                const std::string& section,
                const std::string& key,
                int defaultValue) const;

    /**
     * @brief 从INI文件读取bool值
     */
    bool readBool(const std::string& filePath,
                  const std::string& section,
                  const std::string& key,
                  bool defaultValue) const;

    /**
     * @brief 写入double值到INI文件
     */
    Result<void> writeDouble(const std::string& filePath,
                             const std::string& section,
                             const std::string& key,
                             double value) const;

    /**
     * @brief 写入int值到INI文件
     */
    Result<void> writeInt(const std::string& filePath,
                          const std::string& section,
                          const std::string& key,
                          int value) const;

    /**
     * @brief 写入bool值到INI文件
     */
    Result<void> writeBool(const std::string& filePath,
                           const std::string& section,
                           const std::string& key,
                           bool value) const;
};

}  // namespace Siligen::Infrastructure::Configs


