#pragma once

#include "domain/diagnostics/aggregates/TestConfiguration.h"
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

/**
 * @brief 测试配置管理接口
 *
 * 定义测试配置的加载、保存和验证操作。
 * 配置文件格式: INI (config/hardware_test_config.ini)
 *
 * 实现者:
 * - ConfigFileAdapter (packages/runtime-host/src/runtime/configuration/) - INI文件实现
 * - MockTestConfigManager (tests/mocks/) - 内存Mock实现
 *
 * 消费者:
 * - 各测试用例类 (application/usecases/)
 * - 配置管理服务 (application/Services/)
 */
class ITestConfigurationPort {
   public:
    virtual ~ITestConfigurationPort() = default;

    // ============ 配置加载与保存 ============

    /**
     * @brief 从文件加载配置
     * @param configFilePath 配置文件路径
     * @return 测试配置对象或错误信息
     */
    virtual Result<TestConfiguration> loadConfig(const std::string& configFilePath) = 0;

    /**
     * @brief 保存配置到文件
     * @param config 测试配置对象
     * @param configFilePath 配置文件路径
     * @return Success或错误信息
     */
    virtual Result<void> saveConfig(const TestConfiguration& config, const std::string& configFilePath) = 0;

    /**
     * @brief 加载默认配置
     * @return 使用默认值的测试配置对象
     */
    virtual TestConfiguration loadDefaultConfig() const = 0;


    // ============ 配置验证 ============

    /**
     * @brief 验证配置有效性
     * @param config 待验证的配置对象
     * @return Success或包含验证错误信息的Result
     */
    virtual Result<void> validateConfig(const TestConfiguration& config) const = 0;

    /**
     * @brief 检查配置文件是否存在
     * @param configFilePath 配置文件路径
     * @return true=文件存在, false=不存在
     */
    virtual bool configFileExists(const std::string& configFilePath) const = 0;


    // ============ 单项配置操作 ============

    /**
     * @brief 获取安全限制配置
     * @return 安全限制参数
     */
    virtual SafetyLimits getSafetyLimits() const = 0;

    /**
     * @brief 设置安全限制配置
     * @param limits 安全限制参数
     * @return Success或错误信息
     */
    virtual Result<void> setSafetyLimits(const SafetyLimits& limits) = 0;

    /**
     * @brief 获取测试参数配置
     * @return 测试参数
     */
    virtual TestParameters getTestParameters() const = 0;

    /**
     * @brief 设置测试参数配置
     * @param params 测试参数
     * @return Success或错误信息
     */
    virtual Result<void> setTestParameters(const TestParameters& params) = 0;

    /**
     * @brief 获取显示参数配置
     * @return 显示参数
     */
    virtual DisplayParameters getDisplayParameters() const = 0;

    /**
     * @brief 设置显示参数配置
     * @param params 显示参数
     * @return Success或错误信息
     */
    virtual Result<void> setDisplayParameters(const DisplayParameters& params) = 0;


    // ============ 轴限位配置 ============

    /**
     * @brief 获取指定轴的限位配置
     * @param axis 轴号(0-1)
     * @return 轴限位配置或错误信息
     */
    virtual Result<AxisLimits> getAxisLimits(LogicalAxisId axis) const = 0;

    /**
     * @brief 设置指定轴的限位配置
     * @param axis 轴号(0-1)
     * @param limits 轴限位配置
     * @return Success或错误信息
     */
    virtual Result<void> setAxisLimits(LogicalAxisId axis, const AxisLimits& limits) = 0;


    // ============ 配置重置 ============

    /**
     * @brief 重置配置为默认值
     * @return Success或错误信息
     */
    virtual Result<void> resetToDefault() = 0;

    /**
     * @brief 创建默认配置文件
     * @param configFilePath 配置文件路径
     * @return Success或错误信息
     */
    virtual Result<void> createDefaultConfigFile(const std::string& configFilePath) = 0;
};

}  // namespace Siligen::Domain::Diagnostics::Ports



