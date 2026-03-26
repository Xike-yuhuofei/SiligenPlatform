// CMPService.h
// 版本: 1.0.0
// 描述: CMP触发控制业务服务 - 领域层核心业务逻辑
//
// 六边形架构 - 领域层服务
// 依赖方向: 依赖Domain port,不依赖infrastructure层
// Task: T044 - Phase 4 建立清晰的模块边界

#pragma once

#include "domain/dispensing/ports/ITriggerControllerPort.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/types/CMPTypes.h"
#include "shared/types/DomainEvent.h"
#include "shared/types/Error.h"
#include "shared/types/Point2D.h"
#include "shared/types/Result.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace Siligen {
namespace Domain {
namespace Dispensing {
namespace DomainServices {

/// @brief CMP触发控制业务服务
/// @details 封装CMP(比较输出)触发控制的核心业务逻辑,管理触发点配置和验证
///
/// 业务规则:
/// - 触发点位置必须在有效范围内(0到1000000脉冲)
/// - 脉冲宽度范围: 1μs到100000μs (100ms)
/// - 延时时间范围: 0μs到1000000μs (1s)
/// - 触发点按位置排序,避免顺序错误
/// - 触发点数量限制: 1到1000个
///
/// 性能约束:
/// - 配置验证时间: <1ms
/// - 触发点查询时间: <0.1ms
/// - 批量配置时间: <5ms
///
/// 使用示例:
/// @code
/// auto cmpService = std::make_shared<CMPService>(triggerPort, logger);
/// CMPConfiguration config;
/// config.trigger_mode = CMPTriggerMode::SEQUENCE;
/// config.trigger_points.push_back(CMPTriggerPoint(1000, DispensingAction::PULSE));
/// auto result = cmpService->ConfigureCMP(config);
/// if (result.IsSuccess()) {
///     // 配置成功
/// }
/// @endcode
class CMPService {
   public:
    using EventCallback = std::function<void(const Siligen::Shared::Types::DomainEvent&)>;

    /// @brief 构造函数
    /// @param trigger_port CMP触发端口
    /// @param logging_service 日志服务接口
    explicit CMPService(std::shared_ptr<Siligen::Domain::Dispensing::Ports::ITriggerControllerPort> trigger_port,
                        std::shared_ptr<Siligen::Shared::Interfaces::ILoggingService> logging_service);

    ~CMPService() = default;

    // 禁止拷贝和移动 (单例模式或唯一所有权)
    CMPService(const CMPService&) = delete;
    CMPService& operator=(const CMPService&) = delete;
    CMPService(CMPService&&) = delete;
    CMPService& operator=(CMPService&&) = delete;

    // ============================================================
    // CMP配置管理
    // ============================================================

    /// @brief 配置CMP触发参数
    /// @param config CMP配置参数
    /// @return Result<void> 配置结果
    /// @note 会验证所有触发点参数,按位置排序后应用配置
    Siligen::Shared::Types::Result<void> ConfigureCMP(const Siligen::Shared::Types::CMPConfiguration& config);

    /// @brief 获取当前CMP配置
    /// @return 当前CMP配置参数
    const Siligen::Shared::Types::CMPConfiguration& GetCMPConfiguration() const;

    /// @brief 启用CMP触发
    /// @return Result<void> 启用结果
    /// @note 启用后,在配置的触发点会自动输出信号
    Siligen::Shared::Types::Result<void> EnableCMP();

    /// @brief 禁用CMP触发
    /// @return Result<void> 禁用结果
    /// @note 禁用后,不再输出触发信号
    Siligen::Shared::Types::Result<void> DisableCMP();

    /// @brief 检查CMP是否已启用
    /// @return Result<bool> 是否已启用
    Siligen::Shared::Types::Result<bool> IsCMPEnabled() const;

    /// @brief 清除所有触发点配置
    /// @return Result<void> 清除结果
    /// @note 会禁用CMP并清空所有触发点
    Siligen::Shared::Types::Result<void> ClearAllTriggerPoints();

    // ============================================================
    // 触发点管理
    // ============================================================

    /// @brief 添加单个触发点
    /// @param trigger_point 触发点参数
    /// @return Result<void> 添加结果
    /// @note 会验证触发点参数,并按位置插入到正确位置
    Siligen::Shared::Types::Result<void> AddTriggerPoint(const Siligen::Shared::Types::CMPTriggerPoint& trigger_point);

    /// @brief 批量添加触发点
    /// @param trigger_points 触发点列表
    /// @return Result<void> 添加结果
    /// @note 会验证所有触发点,按位置排序后批量添加
    Siligen::Shared::Types::Result<void> AddTriggerPoints(
        const std::vector<Siligen::Shared::Types::CMPTriggerPoint>& trigger_points);

    /// @brief 删除指定位置的触发点
    /// @param position 触发位置(脉冲)
    /// @return Result<void> 删除结果
    /// @note 如果该位置有多个触发点,只删除第一个
    Siligen::Shared::Types::Result<void> RemoveTriggerPoint(int32_t position);

    /// @brief 获取指定位置的触发点
    /// @param position 触发位置(脉冲)
    /// @return Result<CMPTriggerPoint> 触发点参数
    Siligen::Shared::Types::Result<Siligen::Shared::Types::CMPTriggerPoint> GetTriggerPoint(int32_t position) const;

    /// @brief 获取所有触发点
    /// @return Result<vector<CMPTriggerPoint>> 所有触发点列表
    Siligen::Shared::Types::Result<std::vector<Siligen::Shared::Types::CMPTriggerPoint>> GetAllTriggerPoints() const;

    /// @brief 获取触发点数量
    /// @return Result<int32_t> 触发点数量
    Siligen::Shared::Types::Result<int32_t> GetTriggerPointCount() const;

    // ============================================================
    // 触发模式管理
    // ============================================================

    /// @brief 设置触发模式
    /// @param mode 触发模式
    /// @return Result<void> 设置结果
    /// @note 不同模式对触发点的要求不同
    Siligen::Shared::Types::Result<void> SetTriggerMode(Siligen::Shared::Types::CMPTriggerMode mode);

    /// @brief 获取当前触发模式
    /// @return Result<CMPTriggerMode> 当前触发模式
    Siligen::Shared::Types::Result<Siligen::Shared::Types::CMPTriggerMode> GetTriggerMode() const;

    /// @brief 设置触发范围(用于RANGE模式)
    /// @param start_position 起始位置(脉冲)
    /// @param end_position 结束位置(脉冲)
    /// @return Result<void> 设置结果
    /// @note 仅在RANGE模式下有效
    Siligen::Shared::Types::Result<void> SetTriggerRange(int32_t start_position, int32_t end_position);

    // ============================================================
    // 业务规则验证
    // ============================================================

    /// @brief 验证CMP配置是否有效
    /// @param config CMP配置参数
    /// @return Result<void> 验证结果
    /// @note 检查触发模式、触发点参数、范围等
    Siligen::Shared::Types::Result<void> ValidateCMPConfiguration(
        const Siligen::Shared::Types::CMPConfiguration& config) const;

    /// @brief 验证触发点是否有效
    /// @param trigger_point 触发点参数
    /// @return Result<void> 验证结果
    /// @note 检查位置范围、脉冲宽度、延时时间等
    Siligen::Shared::Types::Result<void> ValidateTriggerPoint(
        const Siligen::Shared::Types::CMPTriggerPoint& trigger_point) const;

    /// @brief 验证触发点数量是否在限制内
    /// @param count 触发点数量
    /// @return Result<void> 验证结果
    /// @note 限制范围: 1到1000个
    Siligen::Shared::Types::Result<void> ValidateTriggerPointCount(int32_t count) const;

    /// @brief 检查触发点位置是否冲突
    /// @param position 待检查的位置
    /// @return Result<bool> 是否有冲突
    /// @note 如果该位置已存在触发点,返回true
    Siligen::Shared::Types::Result<bool> HasTriggerPointConflict(int32_t position) const;

    // ============================================================
    // 工具方法
    // ============================================================

    /// @brief 根据轨迹路径自动生成触发点
    /// @param path 轨迹路径点列表
    /// @param interval 触发间隔(mm)
    /// @param pulse_width 脉冲宽度(μs)
    /// @return Result<vector<CMPTriggerPoint>> 生成的触发点列表
    /// @note 沿轨迹等距离生成触发点
    Siligen::Shared::Types::Result<std::vector<Siligen::Shared::Types::CMPTriggerPoint>> GenerateTriggerPointsFromPath(
        const std::vector<Siligen::Shared::Types::Point2D>& path, float interval, int32_t pulse_width = 2000) const;

    /// @brief 优化触发点配置
    /// @return Result<void> 优化结果
    /// @note 移除重复触发点,合并相邻触发点,按位置排序
    Siligen::Shared::Types::Result<void> OptimizeTriggerPoints();

    /// @brief 导出CMP配置到JSON格式
    /// @return Result<string> JSON格式的配置字符串
    Siligen::Shared::Types::Result<std::string> ExportConfigurationToJson() const;

    /// @brief 从JSON格式导入CMP配置
    /// @param json_config JSON格式的配置字符串
    /// @return Result<void> 导入结果
    Siligen::Shared::Types::Result<void> ImportConfigurationFromJson(const std::string& json_config);

    // ============================================================
    // 事件系统
    // ============================================================

    /// @brief 注册事件回调函数
    /// @param callback 事件回调函数
    /// @note 当CMP配置变化时触发
    void RegisterEventCallback(EventCallback callback);

    /// @brief 清除所有事件回调函数
    void ClearEventCallbacks();

   private:
    // CMP触发端口
    std::shared_ptr<Siligen::Domain::Dispensing::Ports::ITriggerControllerPort> trigger_port_;

    // 日志服务接口
    std::shared_ptr<Siligen::Shared::Interfaces::ILoggingService> logging_service_;

    // CMP配置
    Siligen::Shared::Types::CMPConfiguration cmp_configuration_;

    // 事件回调列表
    std::vector<EventCallback> event_callbacks_;

    // 触发点数量限制常量
    static constexpr int32_t MAX_TRIGGER_POINTS = 1000;
    static constexpr int32_t MIN_TRIGGER_POINTS = 1;

    // 触发参数范围常量
    static constexpr int32_t MIN_POSITION = 0;
    static constexpr int32_t MAX_POSITION = 1000000;
    static constexpr int32_t MIN_PULSE_WIDTH = 1;
    static constexpr int32_t MAX_PULSE_WIDTH = 100000;
    static constexpr int32_t MIN_DELAY_TIME = 0;
    static constexpr int32_t MAX_DELAY_TIME = 1000000;

    // 触发领域事件
    void PublishEvent(const Siligen::Shared::Types::DomainEvent& event);

    // 按位置排序触发点
    void SortTriggerPoints();

    // 记录日志的辅助方法
    void LogInfo(const std::string& message);
    void LogError(const std::string& message);
    void LogDebug(const std::string& message);
};

}  // namespace DomainServices
}  // namespace Dispensing
}  // namespace Domain
}  // namespace Siligen

