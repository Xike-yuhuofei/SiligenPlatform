#pragma once

#include "domain/configuration/ports/IConfigurationPort.h"
#include "domain/motion/ports/IHomingPort.h"
#include "domain/motion/value-objects/HardwareTestTypes.h"
#include "siligen/device/adapters/drivers/multicard/IMultiCardWrapper.h"
#include "shared/types/HardwareConfiguration.h"

#include <array>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Siligen::Infrastructure::Adapters::Motion {

using Siligen::Domain::Configuration::Ports::HomingConfig;
using Siligen::Domain::Motion::Ports::HomingState;
using Siligen::Domain::Motion::Ports::HomingStatus;
using Siligen::Domain::Motion::Ports::IHomingPort;
using Siligen::Domain::Motion::ValueObjects::HomeInputState;
using Siligen::Domain::Motion::ValueObjects::HomingStage;
using Siligen::Domain::Motion::ValueObjects::HomingTestStatus;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Result;

/**
 * @brief 运行时回零适配器
 *
 * 直接基于 MultiCard 与回零配置提供 IHomingPort，
 * 不再通过 diagnostics/maintenance 的 IHardwareTestPort 中转。
 */
class HomingPortAdapter final : public IHomingPort {
   public:
    HomingPortAdapter(std::shared_ptr<Siligen::Infrastructure::Hardware::IMultiCardWrapper> multicard,
                      const Siligen::Shared::Types::HardwareConfiguration& config,
                      const std::array<HomingConfig, 4>& homing_configs);

    Result<void> HomeAxis(LogicalAxisId axis) override;
    Result<void> StopHoming(LogicalAxisId axis) override;
    Result<void> ResetHomingState(LogicalAxisId axis) override;

    Result<HomingStatus> GetHomingStatus(LogicalAxisId axis) const override;
    Result<bool> IsAxisHomed(LogicalAxisId axis) const override;
    Result<bool> IsHomingInProgress(LogicalAxisId axis) const override;

    Result<void> WaitForHomingComplete(LogicalAxisId axis, int32 timeout_ms = 30000) override;

   private:
    struct Units {
        static constexpr double PULSE_PER_SEC_TO_MS = 1000.0;
    };

    static int AxisIndex(LogicalAxisId axis) {
        return static_cast<int>(axis);
    }

    bool isConnected() const;
    Result<void> startHoming(const std::vector<LogicalAxisId>& axes);
    Result<void> stopHoming(const std::vector<LogicalAxisId>& axes);
    HomingTestStatus QueryHomingTestStatus(LogicalAxisId axis) const;
    HomingStage getHomingStage(LogicalAxisId axis) const;
    Result<double> getAxisPosition(LogicalAxisId axis) const;
    Result<void> validateAxisNumber(int axis) const;
    Result<HomeInputState> readHomeInputStateRaw(int axis) const;
    Result<bool> IsHomeInputTriggeredStable(int axis,
                                            int sample_count,
                                            int sample_interval_ms,
                                            bool allow_blocking = true) const;
    Result<void> EscapeFromHomeLimit(int axis, short axis_num, const HomingConfig& config);
    std::string getMultiCardErrorMessage(short error_code) const;

    static HomingState ConvertStatus(HomingTestStatus status);
    bool IsHomingInvalidated(LogicalAxisId axis) const;
    void SetHomingInvalidated(LogicalAxisId axis, bool invalidated);

    std::shared_ptr<Siligen::Infrastructure::Hardware::IMultiCardWrapper> multicard_;
    Siligen::Shared::Types::HardwareConfiguration hardware_config_;
    Siligen::Shared::Types::UnitConverter unit_converter_;
    std::array<HomingConfig, 4> homing_configs_{};
    int axis_count_ = 0;

    mutable std::recursive_mutex m_mutex;
    mutable std::array<bool, 4> homing_success_latch_{};
    mutable std::array<bool, 4> homing_failed_latch_{};
    mutable std::array<bool, 4> homing_active_latch_{};
    mutable std::array<HomingStage, 4> homing_stage_{};
    mutable std::array<std::chrono::steady_clock::time_point, 4> homing_start_time_{};
    mutable std::array<bool, 4> homing_timer_active_{};
    mutable std::array<bool, 4> homing_invalidated_{};
};

}  // namespace Siligen::Infrastructure::Adapters::Motion

