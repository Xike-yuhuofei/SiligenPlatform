// LoggingServiceLocator.h
// Phase 3: 六边形架构日志系统 - 全局日志服务定位器
//
// 提供全局访问 ILoggingService 的能力，用于宏定义和向后兼容
// 这是一个过渡方案，最终应通过依赖注入传递日志服务

#pragma once

#include <memory>
#include <mutex>

namespace Siligen {
namespace Shared {
namespace Interfaces {
class ILoggingService;
}
}
}

namespace Siligen::Shared::DI {

/// @brief 日志服务定位器 - 单例模式
/// @details 提供全局访问 ILoggingService 的能力
///
/// 使用场景:
/// - 日志宏定义 (SILIGEN_LOG_*)
/// - 向后兼容旧代码
/// - 不支持依赖注入的遗留模块
///
/// 注意: 这是过渡方案，新代码应使用依赖注入
class LoggingServiceLocator {
public:
    /// @brief 获取单例实例
    static LoggingServiceLocator& GetInstance() {
        static LoggingServiceLocator instance;
        return instance;
    }

    /// @brief 设置日志服务
    /// @param service 日志服务接口指针
    /// @note 通常在 ApplicationContainer 初始化时调用
    void SetService(std::shared_ptr<Interfaces::ILoggingService> service) {
        std::lock_guard<std::mutex> lock(mutex_);
        service_ = service;
    }

    /// @brief 获取日志服务
    /// @return 日志服务接口指针 (可能为nullptr)
    std::shared_ptr<Interfaces::ILoggingService> GetService() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return service_;
    }

    /// @brief 检查是否有日志服务
    bool HasService() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return service_ != nullptr;
    }

    /// @brief 清除日志服务 (程序退出时)
    void ClearService() {
        std::lock_guard<std::mutex> lock(mutex_);
        service_.reset();
    }

private:
    LoggingServiceLocator() = default;
    ~LoggingServiceLocator() = default;

    // 禁止拷贝和移动
    LoggingServiceLocator(const LoggingServiceLocator&) = delete;
    LoggingServiceLocator& operator=(const LoggingServiceLocator&) = delete;

    mutable std::mutex mutex_;
    std::shared_ptr<Interfaces::ILoggingService> service_;
};

}  // namespace Siligen::Shared::DI
