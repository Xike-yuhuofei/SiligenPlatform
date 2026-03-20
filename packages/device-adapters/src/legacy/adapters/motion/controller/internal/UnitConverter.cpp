#include "UnitConverter.h"

#include "shared/types/HardwareConfiguration.h"

#include <cstdio>
#include <stdexcept>

namespace Siligen::Infrastructure::Adapters {

UnitConverter::UnitConverter(const std::array<double, 8>& pulses_per_mm) : pulses_per_mm_(pulses_per_mm) {
    // 验证配置
    for (size_t i = 0; i < 8; ++i) {
        if (pulses_per_mm_[i] <= 0.0) {
            std::fprintf(stderr, "[UnitConverter] Invalid pulses_per_mm for axis %zu: %f\n", i, pulses_per_mm_[i]);
            pulses_per_mm_[i] = 1000.0;  // 默认值
        }
    }
}

UnitConverter::UnitConverter(const Shared::Types::HardwareConfiguration& config) : pulses_per_mm_() {
    // 从 HardwareConfiguration 的单一 pulse_per_mm 值初始化所有轴
    double pulse_per_mm = static_cast<double>(config.pulse_per_mm);
    for (size_t i = 0; i < 8; ++i) {
        pulses_per_mm_[i] = pulse_per_mm;
    }
}

long UnitConverter::PositionToPulses(size_t axis_index, double position_mm) const noexcept {
    if (!IsValidAxis(axis_index)) return 0;
    return static_cast<long>(position_mm * pulses_per_mm_[axis_index]);
}

double UnitConverter::PulsesToPosition(size_t axis_index, long pulses) const noexcept {
    if (!IsValidAxis(axis_index)) return 0.0;
    return static_cast<double>(pulses) / pulses_per_mm_[axis_index];
}

long UnitConverter::DistanceToPulses(size_t axis_index, double distance_mm) const noexcept {
    return PositionToPulses(axis_index, distance_mm);
}

double UnitConverter::PulsesToDistance(size_t axis_index, long pulses) const noexcept {
    return PulsesToPosition(axis_index, pulses);
}

double UnitConverter::VelocityToPulsePerMs(size_t axis_index, double velocity_mm_s) const noexcept {
    if (!IsValidAxis(axis_index)) return 0.0;
    // mm/s -> pulse/s -> pulse/ms
    double pulse_per_s = velocity_mm_s * pulses_per_mm_[axis_index];
    return pulse_per_s / 1000.0;
}

double UnitConverter::PulsePerMsToVelocity(size_t axis_index, double vel_pulse_ms) const noexcept {
    if (!IsValidAxis(axis_index)) return 0.0;
    // pulse/ms -> pulse/s -> mm/s
    double pulse_per_s = vel_pulse_ms * 1000.0;
    return pulse_per_s / pulses_per_mm_[axis_index];
}

double UnitConverter::AccelerationToPulsePerMs2(size_t axis_index, double accel_mm_s2) const noexcept {
    if (!IsValidAxis(axis_index)) return 0.0;
    // mm/s² -> pulse/s² -> pulse/ms²
    double pulse_per_s2 = accel_mm_s2 * pulses_per_mm_[axis_index];
    return pulse_per_s2 / 1000000.0;  // / 1000²
}

double UnitConverter::PulsePerMs2ToAcceleration(size_t axis_index, double accel_pulse_ms2) const noexcept {
    if (!IsValidAxis(axis_index)) return 0.0;
    // pulse/ms² -> pulse/s² -> mm/s²
    double pulse_per_s2 = accel_pulse_ms2 * 1000000.0;  // * 1000²
    return pulse_per_s2 / pulses_per_mm_[axis_index];
}

bool UnitConverter::IsValidAxis(size_t axis_index) const noexcept {
    if (axis_index >= 8) {
        std::fprintf(stderr, "[UnitConverter] Invalid axis index: %zu\n", axis_index);
        return false;
    }
    return true;
}

double UnitConverter::GetPulsesPerMm(size_t axis_index) const noexcept {
    if (!IsValidAxis(axis_index)) return 1000.0;
    return pulses_per_mm_[axis_index];
}

}  // namespace Siligen::Infrastructure::Adapters


