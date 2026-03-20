// IHardwareController.h
// 版本: 1.1.0
// 描述: 硬件控制器接口契约 - 定义运动控制和硬件操作的标准接口
// 变更历史: v1.1.0 - 新增轨迹执行接口（7个方法）
//
// 六边形架构 - 端口层接口
// 依赖方向: Infrastructure层和Adapter层依赖此接口,而非具体实现

#pragma once

#include "../util/ModuleDependencyCheck.h"

#include <cstdint>
#include <memory>
#include <vector>

// 前向声明 - 避免循环依赖
namespace Siligen::Shared::Types {
template <typename T>
class Result;
class Error;
struct Point2D;
struct AxisStatus;
struct ConnectionConfig;
enum class AxisState : int32_t;
enum class LogicalAxisId : int16_t;
}  // namespace Siligen::Shared::Types

namespace Siligen::Shared::Interfaces {

/// @brief 硬件控制器接口
/// @details 定义所有硬件控制操作的抽象接口,支持依赖注入和单元测试
///
/// 性能约束:
/// - 最大响应时间: 10ms
/// - 内存开销限制: 1KB
/// - 关键路径: 零开销抽象
/// - 禁止阻塞操作
///
/// 使用示例:
/// @code
/// auto controller = ServiceLocator::Get<IHardwareController>();
/// ConnectionConfig config;
/// config.local_ip = "192.168.0.200";  // 本机 IP（必须在控制卡网段内）
/// config.card_ip = "192.168.0.1";     // 控制卡 IP
/// auto result = controller->Connect(config);
/// if (result.IsSuccess()) {
///     // 连接成功
/// }
/// @endcode
class IHardwareController : public Siligen::Shared::Util::SharedModuleTag {
   public:
    virtual ~IHardwareController() = default;

    // ============================================================
    // 连接管理接口
    // ============================================================

    /// @brief 连接到硬件控制卡
    /// @param config 连接配置信息
    /// @return Result<void> 连接结果
    /// @note 线程安全,可在任意线程调用
    virtual Siligen::Shared::Types::Result<void> Connect(const Siligen::Shared::Types::ConnectionConfig& config) = 0;

    /// @brief 断开硬件连接
    /// @return Result<void> 断开结果
    /// @note 线程安全,自动释放所有资源
    virtual Siligen::Shared::Types::Result<void> Disconnect() = 0;

    /// @brief 检查连接状态
    /// @return Result<bool> 连接状态(true=已连接, false=未连接)
    /// @note 零开销调用,不执行I/O操作
    virtual Siligen::Shared::Types::Result<bool> IsConnected() const = 0;

    // ============================================================
    // 轴控制接口
    // ============================================================

    /// @brief 启用指定轴
    /// @param axis_id 逻辑轴号 (0-based)
    /// @return Result<void> 启用结果
    /// @pre 必须已连接硬件
    /// @note 启用后轴进入ENABLED状态
    virtual Siligen::Shared::Types::Result<void> EnableAxis(Siligen::Shared::Types::LogicalAxisId axis_id) = 0;

    /// @brief 禁用指定轴
    /// @param axis_id 逻辑轴号 (0-based)
    /// @return Result<void> 禁用结果
    /// @note 禁用后轴进入DISABLED状态,释放伺服力矩
    virtual Siligen::Shared::Types::Result<void> DisableAxis(Siligen::Shared::Types::LogicalAxisId axis_id) = 0;

    /// @brief 移动到指定二维位置(两轴联动)
    /// @param position 目标位置坐标
    /// @param speed 移动速度 (mm/s), 默认值由配置决定
    /// @return Result<void> 移动结果
    /// @pre 相关轴必须已回零(is_homed=true)
    /// @note 最大速度限制: 10.0 mm/s (硬编码安全约束)
    virtual Siligen::Shared::Types::Result<void> MoveToPosition(const Siligen::Shared::Types::Point2D& position,
                                                                float speed = -1.0f) = 0;

    /// @brief 移动单个轴到指定位置
    /// @param axis_id 逻辑轴号 (0-based)
    /// @param position 目标位置 (mm)
    /// @param speed 移动速度 (mm/s), 默认值由配置决定
    /// @return Result<void> 移动结果
    /// @pre 轴必须已回零(is_homed=true)
    /// @note 最大速度限制: 10.0 mm/s (硬编码安全约束)
    virtual Siligen::Shared::Types::Result<void> MoveAxisToPosition(Siligen::Shared::Types::LogicalAxisId axis_id,
                                                                    float position,
                                                                    float speed = -1.0f) = 0;

    /// @brief 停止指定轴运动
    /// @param axis_id 逻辑轴号 (0-based)
    /// @return Result<void> 停止结果
    /// @note 使用减速停止,非急停
    virtual Siligen::Shared::Types::Result<void> StopAxis(Siligen::Shared::Types::LogicalAxisId axis_id) = 0;

    /// @brief 紧急停止所有轴
    /// @return Result<void> 紧急停止结果
    /// @note 立即停止所有运动,响应时间<100ms
    /// @note 所有轴状态变为EMERGENCY_STOP
    virtual Siligen::Shared::Types::Result<void> EmergencyStop() = 0;

    // ============================================================
    // 轨迹执行接口
    // ============================================================

    /// @brief 获取坐标系缓冲区可用空间
    /// @param crd_num 坐标系编号 (1或2)
    /// @return Result<int32_t> 缓冲区可用空间（指令数量）
    /// @pre 必须已连接硬件且坐标系已初始化
    /// @note 用于流式轨迹执行前的缓冲区检查
    virtual Siligen::Shared::Types::Result<int32_t> GetCoordinateBufferSpace(int32_t crd_num) const = 0;

    /// @brief 清空坐标系缓冲区
    /// @param crd_num 坐标系编号 (1或2)
    /// @return Result<void> 清空结果
    /// @note 停止当前轨迹执行并清空所有待执行指令
    virtual Siligen::Shared::Types::Result<void> ClearCoordinateBuffer(int32_t crd_num) = 0;

    /// @brief 执行直线插补运动
    /// @param crd_num 坐标系编号 (1或2)
    /// @param x_pulse X轴目标位置（脉冲）
    /// @param y_pulse Y轴目标位置（脉冲）
    /// @param velocity 合成速度（脉冲/ms）
    /// @param acceleration 加速度（脉冲/ms²）
    /// @param end_velocity 终点速度（脉冲/ms）
    /// @param fifo FIFO模式（0=立即执行，1=FIFO缓冲）
    /// @param segment_id 段号（用于轨迹跟踪）
    /// @return Result<void> 执行结果
    /// @pre 坐标系必须已初始化，缓冲区有足够空间
    /// @note 指令添加到缓冲区，需调用StartCoordinateMotion启动执行
    virtual Siligen::Shared::Types::Result<void> ExecuteLinearInterpolation(int32_t crd_num,
                                                                            int32_t x_pulse,
                                                                            int32_t y_pulse,
                                                                            double velocity,
                                                                            double acceleration,
                                                                            double end_velocity,
                                                                            int16_t fifo,
                                                                            int32_t segment_id) = 0;

    /// @brief 执行圆弧插补运动
    /// @param crd_num 坐标系编号 (1或2)
    /// @param x_pulse X轴目标位置（脉冲）
    /// @param y_pulse Y轴目标位置（脉冲）
    /// @param cx 圆心X坐标（脉冲，相对当前位置）
    /// @param cy 圆心Y坐标（脉冲，相对当前位置）
    /// @param direction 圆弧方向（0=顺时针CW，1=逆时针CCW）
    /// @param velocity 合成速度（脉冲/ms）
    /// @param acceleration 加速度（脉冲/ms²）
    /// @param end_velocity 终点速度（脉冲/ms）
    /// @param fifo FIFO模式（0=立即执行，1=FIFO缓冲）
    /// @param segment_id 段号（用于轨迹跟踪）
    /// @return Result<void> 执行结果
    /// @pre 坐标系必须已初始化，缓冲区有足够空间
    /// @note 指令添加到缓冲区，需调用StartCoordinateMotion启动执行
    virtual Siligen::Shared::Types::Result<void> ExecuteArcInterpolation(int32_t crd_num,
                                                                         int32_t x_pulse,
                                                                         int32_t y_pulse,
                                                                         double cx,
                                                                         double cy,
                                                                         int16_t direction,
                                                                         double velocity,
                                                                         double acceleration,
                                                                         double end_velocity,
                                                                         int16_t fifo,
                                                                         int32_t segment_id) = 0;

    /// @brief 启动坐标系运动
    /// @param crd_num 坐标系编号 (1或2)
    /// @return Result<void> 启动结果
    /// @pre 缓冲区中必须有待执行的轨迹指令
    /// @note 开始执行缓冲区中的所有轨迹指令
    virtual Siligen::Shared::Types::Result<void> StartCoordinateMotion(int32_t crd_num) = 0;

    /// @brief 停止运动
    /// @param stop_mode 停止模式（-1=正常减速停止，0=急停）
    /// @return Result<void> 停止结果
    /// @note 影响所有轴和坐标系
    virtual Siligen::Shared::Types::Result<void> StopMotion(int32_t stop_mode) = 0;

    /// @brief 触发比较输出（CMP脉冲）
    /// @param channel_mask 通道掩码（位域，每位代表一个通道）
    /// @param output_mode 输出模式（2=脉冲输出）
    /// @param output_pulse_count 输出脉冲数（0=单脉冲）
    /// @param pulse_width_us 脉冲宽度（微秒）
    /// @param level_before 脉冲前电平
    /// @param level_during 脉冲期间电平
    /// @param level_after 脉冲后电平
    /// @return Result<void> 触发结果
    /// @pre 比较输出功能必须已配置
    /// @note 用于位置触发的点胶控制
    virtual Siligen::Shared::Types::Result<void> TriggerCompareOutput(uint16_t channel_mask,
                                                                      int32_t output_mode,
                                                                      int32_t output_pulse_count,
                                                                      int32_t pulse_width_us,
                                                                      int32_t level_before,
                                                                      int32_t level_during,
                                                                      int32_t level_after) = 0;

    // ============================================================
    // 状态查询接口
    // ============================================================

    /// @brief 获取指定轴的状态信息
    /// @param axis_id 逻辑轴号 (0-based)
    /// @return Result<AxisStatus> 轴状态信息
    /// @note 实时查询,性能关键路径
    virtual Siligen::Shared::Types::Result<Siligen::Shared::Types::AxisStatus> GetAxisStatus(
        Siligen::Shared::Types::LogicalAxisId axis_id) const = 0;

    /// @brief 获取所有轴的状态信息
    /// @return Result<vector<AxisStatus>> 所有轴状态列表
    /// @note 批量查询,减少调用次数
    virtual Siligen::Shared::Types::Result<std::vector<Siligen::Shared::Types::AxisStatus>> GetAllAxisStatus()
        const = 0;

    /// @brief 获取当前二维位置(前两轴)
    /// @return Result<Point2D> 当前位置坐标
    /// @note 高频调用,零开销抽象优化
    virtual Siligen::Shared::Types::Result<Siligen::Shared::Types::Point2D> GetCurrentPosition() const = 0;

    // ============================================================
    // 接口元数据
    // ============================================================

    /// @brief 获取接口版本号
    /// @return 版本字符串 (语义化版本)
    virtual const char* GetInterfaceVersion() const noexcept {
        return "1.1.0";
    }

    /// @brief 获取实现类型名称
    /// @return 实现类型名称
    /// @note 用于调试和日志记录
    virtual const char* GetImplementationType() const noexcept = 0;
};

}  // namespace Siligen::Shared::Interfaces
