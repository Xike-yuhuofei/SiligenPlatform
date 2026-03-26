#pragma once

#include "domain/machine/ports/IHardwareTestPort.h"
#include "domain/dispensing/ports/ITriggerControllerPort.h"
#include "domain/motion/value-objects/HardwareTestTypes.h"
#include "shared/types/Result.h"

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Siligen::Infrastructure::Adapters::Dispensing {

using Siligen::Domain::Dispensing::Ports::ITriggerControllerPort;
using Siligen::Domain::Dispensing::Ports::TriggerConfig;
using Siligen::Domain::Dispensing::Ports::TriggerMode;
using Siligen::Domain::Dispensing::Ports::TriggerStatus;
using Siligen::Domain::Machine::Ports::IHardwareTestPort;
using Siligen::Domain::Motion::ValueObjects::TriggerAction;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Result;

/**
 * @brief 触发控制端口适配器
 *
 * 基于 IHardwareTestPort 的触发能力实现 ITriggerControllerPort。
 */
class TriggerControllerAdapter final : public ITriggerControllerPort {
   public:
    explicit TriggerControllerAdapter(std::shared_ptr<IHardwareTestPort> hardware_test_port);

    Result<void> ConfigureTrigger(const TriggerConfig& config) override;
    Result<TriggerConfig> GetTriggerConfig() const override;

    Result<void> SetSingleTrigger(LogicalAxisId axis, float32 position, int32 pulse_width_us) override;
    Result<void> SetContinuousTrigger(LogicalAxisId axis,
                                      float32 start_pos,
                                      float32 end_pos,
                                      float32 interval,
                                      int32 pulse_width_us) override;
    Result<void> SetRangeTrigger(LogicalAxisId axis, float32 start_pos, float32 end_pos, int32 pulse_width_us) override;
    Result<void> SetSequenceTrigger(LogicalAxisId axis,
                                    const std::vector<float32>& positions,
                                    int32 pulse_width_us) override;

    Result<void> EnableTrigger(LogicalAxisId axis) override;
    Result<void> DisableTrigger(LogicalAxisId axis) override;
    Result<void> ClearTrigger(LogicalAxisId axis) override;

    Result<TriggerStatus> GetTriggerStatus(LogicalAxisId axis) const override;
    Result<bool> IsTriggerEnabled(LogicalAxisId axis) const override;
    Result<int32> GetTriggerCount(LogicalAxisId axis) const override;

   private:
    struct AxisTriggerState {
        std::vector<int> trigger_ids;
        std::vector<float32> trigger_positions;
        bool enabled = false;
    };

    std::shared_ptr<IHardwareTestPort> hardware_test_port_;
    TriggerConfig config_;
    mutable std::mutex mutex_;
    std::unordered_map<short, AxisTriggerState> trigger_states_;

    Result<void> AddTriggerPoint(LogicalAxisId axis, float32 position, TriggerAction action);
};

}  // namespace Siligen::Infrastructure::Adapters::Dispensing




