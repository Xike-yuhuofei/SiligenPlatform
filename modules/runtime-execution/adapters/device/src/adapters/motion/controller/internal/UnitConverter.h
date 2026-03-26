#pragma once

#include <array>
#include <cstdint>

// 前向声明
namespace Siligen {
namespace Shared {
namespace Types {
struct HardwareConfiguration;
}
}  // namespace Shared
}  // namespace Siligen

namespace Siligen::Infrastructure::Adapters {

/**
 * @brief 单位转换器 - 集中管理 Domain 层与硬件层之间的单位转换
 *
 * 职责:
 * - Domain 层使用标准单位 (mm, mm/s, mm/s²)
 * - 硬件层使用脉冲单位 (pulse, pulse/s, pulse/ms)
 * - 此类负责双向转换，隐藏硬件实现细节
 */
class UnitConverter {
   public:
    /**
     * @brief 构造函数 - 从每轴独立的脉冲配置构造
     * @param pulses_per_mm 每轴的脉冲/毫米比值 (长度 8，对应 X,Y,Z,A,B,C,U,V)
     */
    explicit UnitConverter(const std::array<double, 8>& pulses_per_mm);

    /**
     * @brief 构造函数 - 从硬件配置构造 (使用统一的 pulse_per_mm)
     * @param config 硬件配置
     */
    explicit UnitConverter(const Shared::Types::HardwareConfiguration& config);

    // === 位置转换 ===

    /// mm 转 pulse (绝对位置)
    long PositionToPulses(size_t axis_index, double position_mm) const noexcept;

    /// pulse 转 mm (绝对位置)
    double PulsesToPosition(size_t axis_index, long pulses) const noexcept;

    /// mm 转 pulse (相对位移)
    long DistanceToPulses(size_t axis_index, double distance_mm) const noexcept;

    /// pulse 转 mm (相对位移)
    double PulsesToDistance(size_t axis_index, long pulses) const noexcept;

    // === 速度转换 ===

    /// mm/s 转 pulse/ms (MultiCard SDK 要求的单位)
    double VelocityToPulsePerMs(size_t axis_index, double velocity_mm_s) const noexcept;

    /// pulse/ms 转 mm/s
    double PulsePerMsToVelocity(size_t axis_index, double vel_pulse_ms) const noexcept;

    // === 加速度转换 ===

    /// mm/s² 转 pulse/ms²
    double AccelerationToPulsePerMs2(size_t axis_index, double accel_mm_s2) const noexcept;

    /// pulse/ms² 转 mm/s²
    double PulsePerMs2ToAcceleration(size_t axis_index, double accel_pulse_ms2) const noexcept;

    // === 验证 ===

    /// 检查轴索引是否有效
    bool IsValidAxis(size_t axis_index) const noexcept;

    /// 获取指定轴的脉冲/毫米比值
    double GetPulsesPerMm(size_t axis_index) const noexcept;

   private:
    std::array<double, 8> pulses_per_mm_;  // 每轴独立配置
};

}  // namespace Siligen::Infrastructure::Adapters
