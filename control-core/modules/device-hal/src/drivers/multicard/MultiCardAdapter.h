// MultiCardAdapter.h
// 版本: 1.0.0
// 描述: MultiCard硬件适配器 - 基础设施层硬件接口实现
//
// 六边形架构 - 基础设施层适配器
// 依赖方向: 实现shared接口层的IHardwareController,调用MultiCard SDK
// Task: T049 - Phase 4 建立清晰的模块边界

#pragma once

#include "shared/interfaces/IHardwareController.h"
#include "shared/types/AxisStatus.h"
#include "shared/types/ConfigTypes.h"
#include "shared/types/Error.h"
#include "shared/types/HardwareConfiguration.h"
#include "shared/types/Point2D.h"
#include "shared/types/Result.h"

// MultiCard SDK C++ 类（厂商官方头文件）
#include "MultiCardCPP.h"

// MockMultiCard 实现
#include "MockMultiCard.h"

// IMultiCardWrapper 接口
#include "IMultiCardWrapper.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace Siligen {
namespace Infrastructure {
namespace Hardware {

/// @brief MultiCard硬件适配器
/// @details 实现IHardwareController接口,封装MultiCard运动控制卡的底层API调用
///
/// 适配器模式:
/// - 接口适配: IHardwareController → MultiCard C API
/// - 错误转换: MultiCard错误码 → Result<T>类型
/// - 类型转换: Point2D/AxisStatus → MultiCard原生类型
/// - 线程安全: 所有硬件操作加锁保护
///
/// 性能约束:
/// - 连接响应时间: <500ms
/// - 轴控制响应时间: <10ms
/// - 状态查询响应时间: <5ms
/// - 紧急停止响应时间: <100ms
///
/// 错误处理:
/// - 所有MultiCard API调用都检查返回值
/// - 错误码映射到统一的Error类型
/// - 详细错误信息记录到日志
///
/// 线程安全:
/// - 所有公共方法使用互斥锁保护
/// - 状态查询使用共享锁(未来优化)
///
/// 使用示例:
/// @code
/// auto adapter = std::make_shared<MultiCardAdapter>();
/// ConnectionConfig config;
/// config.local_ip = "192.168.0.200";  // 本机 IP（必须在控制卡网段内）
/// config.card_ip = "192.168.0.1";     // 控制卡 IP
/// auto result = adapter->Connect(config);
/// if (result.IsSuccess()) {
///     adapter->EnableAxis(0);
/// }
/// @endcode
class MultiCardAdapter : public Siligen::Shared::Interfaces::IHardwareController {
   public:
    /// @brief 构造函数
    /// @param multicard 可选的共享IMultiCardWrapper实例（类型安全接口）
    /// @param config 硬件配置（包含单位转换系数）
    explicit MultiCardAdapter(
        std::shared_ptr<IMultiCardWrapper> multicard,
        const Siligen::Shared::Types::HardwareConfiguration& config);

    /// @brief 析构函数
    /// @note 自动断开连接并释放资源
    ~MultiCardAdapter() override;

    // 禁止拷贝和移动
    MultiCardAdapter(const MultiCardAdapter&) = delete;
    MultiCardAdapter& operator=(const MultiCardAdapter&) = delete;
    MultiCardAdapter(MultiCardAdapter&&) = delete;
    MultiCardAdapter& operator=(MultiCardAdapter&&) = delete;

    // ============================================================
    // IHardwareController接口实现
    // ============================================================

    // 连接管理
    Siligen::Shared::Types::Result<void> Connect(const Siligen::Shared::Types::ConnectionConfig& config) override;

    Siligen::Shared::Types::Result<void> Disconnect() override;

    Siligen::Shared::Types::Result<bool> IsConnected() const override;

    // 轴控制
    Siligen::Shared::Types::Result<void> EnableAxis(Siligen::Shared::Types::LogicalAxisId axis_id) override;

    Siligen::Shared::Types::Result<void> DisableAxis(Siligen::Shared::Types::LogicalAxisId axis_id) override;

    Siligen::Shared::Types::Result<void> MoveToPosition(const Siligen::Shared::Types::Point2D& position,
                                                        float speed = -1.0f) override;

    Siligen::Shared::Types::Result<void> MoveAxisToPosition(Siligen::Shared::Types::LogicalAxisId axis_id,
                                                            float position,
                                                            float speed = -1.0f) override;

    Siligen::Shared::Types::Result<void> StopAxis(Siligen::Shared::Types::LogicalAxisId axis_id) override;

    Siligen::Shared::Types::Result<void> EmergencyStop() override;

    // 轨迹执行
    Siligen::Shared::Types::Result<int32_t> GetCoordinateBufferSpace(int32_t crd_num) const override;

    Siligen::Shared::Types::Result<void> ClearCoordinateBuffer(int32_t crd_num) override;

    Siligen::Shared::Types::Result<void> ExecuteLinearInterpolation(int32_t crd_num,
                                                                    int32_t x_pulse,
                                                                    int32_t y_pulse,
                                                                    double velocity,
                                                                    double acceleration,
                                                                    double end_velocity,
                                                                    int16_t fifo,
                                                                    int32_t segment_id) override;

    Siligen::Shared::Types::Result<void> ExecuteArcInterpolation(int32_t crd_num,
                                                                 int32_t x_pulse,
                                                                 int32_t y_pulse,
                                                                 double cx,
                                                                 double cy,
                                                                 int16_t direction,
                                                                 double velocity,
                                                                 double acceleration,
                                                                 double end_velocity,
                                                                 int16_t fifo,
                                                                 int32_t segment_id) override;

    Siligen::Shared::Types::Result<void> StartCoordinateMotion(int32_t crd_num) override;

    Siligen::Shared::Types::Result<void> StopMotion(int32_t stop_mode) override;

    Siligen::Shared::Types::Result<void> TriggerCompareOutput(uint16_t channel_mask,
                                                              int32_t output_mode,
                                                              int32_t output_pulse_count,
                                                              int32_t pulse_width_us,
                                                              int32_t level_before,
                                                              int32_t level_during,
                                                              int32_t level_after) override;

    // 状态查询
    Siligen::Shared::Types::Result<Siligen::Shared::Types::AxisStatus> GetAxisStatus(
        Siligen::Shared::Types::LogicalAxisId axis_id) const override;

    Siligen::Shared::Types::Result<std::vector<Siligen::Shared::Types::AxisStatus>> GetAllAxisStatus() const override;

    Siligen::Shared::Types::Result<Siligen::Shared::Types::Point2D> GetCurrentPosition() const override;

    // 接口元数据
    const char* GetImplementationType() const noexcept override {
        return "MultiCardAdapter";
    }

   private:
    // MultiCard或MockMultiCard包装器（类型安全接口）
    std::shared_ptr<IMultiCardWrapper> multicard_;

    // 连接状态
    bool is_connected_;

    // 配置参数
    Siligen::Shared::Types::ConnectionConfig connection_config_;
    float default_speed_;  // 默认移动速度(mm/s)
    float max_speed_;      // 最大允许速度(mm/s)
    float max_acceleration_;  // 最大允许加速度(mm/s^2)

    // 单位转换器 (统一管理单位转换，替代硬编码PULSE_PER_MM)
    Siligen::Shared::Types::UnitConverter unit_converter_;
    int32_t axis_count_;

    // 线程安全
    mutable std::mutex hardware_mutex_;

    // ============================================================
    // 辅助方法
    // ============================================================

    /// @brief 验证轴ID有效性
    /// @param axis_id 轴ID
    /// @return Result<void> 验证结果
    Siligen::Shared::Types::Result<void> ValidateAxisId(Siligen::Shared::Types::LogicalAxisId axis_id) const;

    /// @brief 将MultiCard错误码转换为Error类型
    /// @param error_code MultiCard API返回的错误码
    /// @param operation 操作名称
    /// @return Error对象
    Siligen::Shared::Types::Error MapMultiCardError(int32_t error_code, const std::string& operation) const;

    /// @brief 将MultiCard轴状态转换为AxisStatus
    /// @param axis_id 轴ID
    /// @param mc_status MultiCard轴状态位
    /// @return AxisStatus对象
    Siligen::Shared::Types::AxisStatus ConvertAxisStatus(Siligen::Shared::Types::LogicalAxisId axis_id,
                                                         uint32_t mc_status) const;

    /// @brief 获取MultiCard轴状态位
    /// @param axis_id 轴ID
    /// @return Result<uint32_t> 状态位
    Siligen::Shared::Types::Result<uint32_t> GetMultiCardAxisStatus(
        Siligen::Shared::Types::LogicalAxisId axis_id) const;

    /// @brief 将mm转换为脉冲
    /// @param mm 毫米值
    /// @return 脉冲值
    /// @note 假设每毫米对应1000个脉冲(可配置)
    int32_t MmToPulse(float mm) const;

    /// @brief 将脉冲转换为mm
    /// @param pulse 脉冲值
    /// @return 毫米值
    float PulseToMm(int32_t pulse) const;

    /// @brief 等待轴到达目标位置
    /// @param axis_id 轴ID
    /// @param timeout_ms 超时时间(毫秒)
    /// @return Result<void> 等待结果
    Siligen::Shared::Types::Result<void> WaitForAxisArrive(Siligen::Shared::Types::LogicalAxisId axis_id,
                                                           int32_t timeout_ms = 30000) const;

    /// @brief 初始化默认坐标系（连接完成后用于插补模块预初始化）
    /// @return Result<void> 初始化结果
    Siligen::Shared::Types::Result<void> InitializeDefaultCoordinateSystem();

    // 常量定义
    static constexpr int32_t MIN_AXIS_ID = 0;
    // PULSE_PER_MM已删除 - 使用unit_converter_统一管理单位转换
};

}  // namespace Hardware
}  // namespace Infrastructure
}  // namespace Siligen

