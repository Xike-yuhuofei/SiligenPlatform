#pragma once

#include "shared/types/CMPTypes.h"
#include "shared/types/Types.h"

#include <chrono>
#include <functional>
#include <memory>
#include <vector>

// 前向声明
class MultiCardInterface;
class MultiCard;

namespace Siligen {

class PositionTriggerController {
   public:
    explicit PositionTriggerController(std::shared_ptr<MultiCardInterface> hw);
    explicit PositionTriggerController(MultiCard* mc);
    ~PositionTriggerController();

    // 初始化和配置
    bool Initialize();
    bool Shutdown();
    bool ConfigureCMPOutput(const CMPPulseConfiguration& config);
    CMPPulseConfiguration GetCMPConfiguration(short channel) const;

    // 触发点管理
    bool SetSinglePointTrigger(int32 position, DispensingAction action, int32 pulse_width_us, int32 delay_time_us = 0);
    bool SetRangeTrigger(int32 start, int32 end, int32 pulse_width_us);
    bool SetSequenceTrigger(const std::vector<CMPTriggerPoint>& points);
    bool SetRepeatTrigger(int32 position, int32 pulse_width_us, int32 interval_us, int32 count);
    bool ClearAllTriggerPoints();
    bool ClearChannelTriggerPoints(short channel);

    // 触发控制
    bool StartTrigger(short channel = 0);  // 0表示所有通道
    bool StopTrigger(short channel = 0);
    bool IsTriggerActive(short channel) const;

    // 状态查询
    CMPOutputStatus GetChannelStatus(short channel) const;
    std::vector<CMPTriggerPoint> GetTriggerPoints(short channel) const;
    int32 GetCurrentPosition() const;

    // 高级功能
    bool EnableTriggerBuffering(short channel, bool enable);
    bool SetTriggerCallback(std::function<void(short, int32, DispensingAction)> callback);

   private:
    std::shared_ptr<MultiCardInterface> hardware_interface_;
    MultiCard* multi_card_direct_;
    std::vector<CMPPulseConfiguration> cmp_configurations_;
    std::vector<std::vector<CMPTriggerPoint>> trigger_points_map_;
    std::vector<CMPOutputStatus> channel_status_map_;
    std::function<void(short, int32, DispensingAction)> trigger_callback_;
    bool is_initialized_;
    bool buffer_enabled_;

    void InitializeChannels();
    void NotifyError(short channel, const std::string& message);
    bool ProcessTriggerPoint(short channel, int32 current_position);
};

}  // namespace Siligen
