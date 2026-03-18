#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Siligen::Shared::Interfaces {
class IConfigurationService;
class IHardwareController;
class ILoggingService;
}

namespace Siligen::Shared::Factories {

/**
 * @brief 适配器工厂
 *
 * 提供统一的适配器创建接口,支持多种适配器类型的动态创建。
 * 使用工厂模式解耦适配器的创建和使用,支持运行时适配器替换。
 * @note 兼容层遗留工厂，新的装配逻辑应优先使用 Composition Root / InfrastructureAdapterFactory。
 *
 * 核心特性:
 * - 类型安全的适配器创建
 * - 支持适配器注册和动态替换
 * - 线程安全的工厂操作
 * - 支持依赖注入集成
 */
class AdapterFactory {
   public:
    /**
     * @brief 适配器类型枚举
     */
    enum class AdapterType {
        HARDWARE_MULTICARD,  // MultiCard硬件适配器
        HARDWARE_MOCK,       // Mock硬件适配器 (测试用)
        LOGGING_CONSOLE,     // 控制台日志适配器
        LOGGING_FILE,        // 文件日志适配器
        CONFIG_INI,          // INI配置适配器
        CONFIG_JSON          // JSON配置适配器
    };

    /**
     * @brief 硬件控制器工厂函数类型
     */
    using HardwareControllerFactory = std::function<std::shared_ptr<Interfaces::IHardwareController>()>;

    /**
     * @brief 日志服务工厂函数类型
     */
    using LoggingServiceFactory = std::function<std::shared_ptr<Interfaces::ILoggingService>()>;

    /**
     * @brief 配置服务工厂函数类型
     */
    using ConfigurationServiceFactory = std::function<std::shared_ptr<Interfaces::IConfigurationService>()>;

   private:
    // 硬件控制器工厂注册表
    static std::unordered_map<AdapterType, HardwareControllerFactory> hardware_factories_;

    // 日志服务工厂注册表
    static std::unordered_map<AdapterType, LoggingServiceFactory> logging_factories_;

    // 配置服务工厂注册表
    static std::unordered_map<AdapterType, ConfigurationServiceFactory> configuration_factories_;

    // 线程安全互斥锁
    static std::mutex mutex_;

    AdapterFactory() = delete;

   public:
    /**
     * @brief 注册硬件控制器工厂
     *
     * @param type 适配器类型
     * @param factory 工厂函数
     */
    static void RegisterHardwareController(AdapterType type, HardwareControllerFactory factory) {
        std::lock_guard<std::mutex> lock(mutex_);
        hardware_factories_[type] = factory;
    }

    /**
     * @brief 注册日志服务工厂
     *
     * @param type 适配器类型
     * @param factory 工厂函数
     */
    static void RegisterLoggingService(AdapterType type, LoggingServiceFactory factory) {
        std::lock_guard<std::mutex> lock(mutex_);
        logging_factories_[type] = factory;
    }

    /**
     * @brief 注册配置服务工厂
     *
     * @param type 适配器类型
     * @param factory 工厂函数
     */
    static void RegisterConfigurationService(AdapterType type, ConfigurationServiceFactory factory) {
        std::lock_guard<std::mutex> lock(mutex_);
        configuration_factories_[type] = factory;
    }

    /**
     * @brief 创建硬件控制器适配器
     *
     * @param type 适配器类型
     * @return 硬件控制器接口的共享指针
     */
    static std::shared_ptr<Interfaces::IHardwareController> CreateHardwareController(AdapterType type) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = hardware_factories_.find(type);
        if (it != hardware_factories_.end()) {
            return it->second();
        }

        return nullptr;
    }

    /**
     * @brief 创建日志服务适配器
     *
     * @param type 适配器类型
     * @return 日志服务接口的共享指针
     */
    static std::shared_ptr<Interfaces::ILoggingService> CreateLoggingService(AdapterType type) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = logging_factories_.find(type);
        if (it != logging_factories_.end()) {
            return it->second();
        }

        return nullptr;
    }

    /**
     * @brief 创建配置服务适配器
     *
     * @param type 适配器类型
     * @return 配置服务接口的共享指针
     */
    static std::shared_ptr<Interfaces::IConfigurationService> CreateConfigurationService(AdapterType type) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = configuration_factories_.find(type);
        if (it != configuration_factories_.end()) {
            return it->second();
        }

        return nullptr;
    }

    /**
     * @brief 检查适配器类型是否已注册
     *
     * @param type 适配器类型
     * @return 是否已注册硬件控制器工厂
     */
    static bool IsHardwareControllerRegistered(AdapterType type) {
        std::lock_guard<std::mutex> lock(mutex_);
        return hardware_factories_.find(type) != hardware_factories_.end();
    }

    /**
     * @brief 检查日志服务适配器是否已注册
     *
     * @param type 适配器类型
     * @return 是否已注册
     */
    static bool IsLoggingServiceRegistered(AdapterType type) {
        std::lock_guard<std::mutex> lock(mutex_);
        return logging_factories_.find(type) != logging_factories_.end();
    }

    /**
     * @brief 检查配置服务适配器是否已注册
     *
     * @param type 适配器类型
     * @return 是否已注册
     */
    static bool IsConfigurationServiceRegistered(AdapterType type) {
        std::lock_guard<std::mutex> lock(mutex_);
        return configuration_factories_.find(type) != configuration_factories_.end();
    }

    /**
     * @brief 清除所有工厂注册
     */
    static void ClearAll() {
        std::lock_guard<std::mutex> lock(mutex_);
        hardware_factories_.clear();
        logging_factories_.clear();
        configuration_factories_.clear();
    }

    /**
     * @brief 获取适配器类型名称
     *
     * @param type 适配器类型
     * @return 类型名称字符串
     */
    static std::string GetAdapterTypeName(AdapterType type) {
        switch (type) {
            case AdapterType::HARDWARE_MULTICARD:
                return "MultiCard硬件适配器";
            case AdapterType::HARDWARE_MOCK:
                return "Mock硬件适配器";
            case AdapterType::LOGGING_CONSOLE:
                return "控制台日志适配器";
            case AdapterType::LOGGING_FILE:
                return "文件日志适配器";
            case AdapterType::CONFIG_INI:
                return "INI配置适配器";
            case AdapterType::CONFIG_JSON:
                return "JSON配置适配器";
            default:
                return "未知适配器";
        }
    }
};

// 静态成员初始化
inline std::unordered_map<AdapterFactory::AdapterType, AdapterFactory::HardwareControllerFactory>
    AdapterFactory::hardware_factories_;

inline std::unordered_map<AdapterFactory::AdapterType, AdapterFactory::LoggingServiceFactory>
    AdapterFactory::logging_factories_;

inline std::unordered_map<AdapterFactory::AdapterType, AdapterFactory::ConfigurationServiceFactory>
    AdapterFactory::configuration_factories_;

inline std::mutex AdapterFactory::mutex_;

}  // namespace Siligen::Shared::Factories
