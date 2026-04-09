#include "siligen/device/adapters/hardware/HardwareTestAdapter.h"

#define MODULE_NAME "HardwareTestAdapter"
#include "shared/interfaces/ILoggingService.h"
#include "shared/types/Error.h"
#include "shared/logging/PrintfLogFormatter.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <thread>

// MultiCard API结构体定义已在MultiCardCPP.h中
#include "siligen/device/adapters/drivers/multicard/MultiCardCPP.h"

namespace Siligen::Infrastructure::Adapters::Hardware {

using namespace Shared::Types;

// ============ 位置触发测试 ============

Result<int> HardwareTestAdapter::configureTriggerPoint(LogicalAxisId axis_id,
                                                       double position,
                                                       int outputPort,
                                                       TriggerAction action) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    TriggerPointConfig config;
    config.id = m_nextTriggerPointId++;
    config.axis = axis_id;
    config.position = position;
    config.outputPort = outputPort;
    config.action = action;
    config.enabled = false;

    m_triggerPoints.push_back(config);

    return Result<int>::Success(config.id);
}

Result<void> HardwareTestAdapter::enableTrigger(int triggerPointId) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    for (auto& point : m_triggerPoints) {
        if (point.id == triggerPointId) {
            // 配置硬件CMP触发
            long positionData[1];
            positionData[0] = static_cast<long>(point.position * 1000);  // 转换为脉冲数(假设1mm=1000脉冲)

            long dummyBuffer[1] = {0};

            // 配置MC_CmpBufData参数
            short encoderNum = ToSdkShort(ToSdkAxis(point.axis));                // SDK轴号从1开始
            short pulseType = 2;                                                 // 脉冲输出
            short startLevel = (point.action == TriggerAction::TurnOn) ? 0 : 1;  // 初始电平
            short pulseTime = 1000;                                              // 脉冲时间1ms
            short posType = 1;                                                   // 绝对位置
            short timeUnit = 1;                                                  // 毫秒单位

            short ret = multicard_->MC_CmpBufData(
                encoderNum, pulseType, startLevel, pulseTime, positionData, 1, dummyBuffer, 0, posType, timeUnit);

            if (ret != 0) {
                return Result<void>::Failure(
                    Error(ErrorCode::HARDWARE_CONNECTION_FAILED,
                          "Failed to configure hardware CMP trigger: " + getMultiCardErrorMessage(ret),
                          "HardwareTestAdapter::enableTrigger"));
            }

            point.enabled = true;
            return Result<void>::Success();
        }
    }

    return Result<void>::Failure(
        Error(ErrorCode::CONFIGURATION_ERROR, "Trigger point not found", "HardwareTestAdapter"));
}

Result<void> HardwareTestAdapter::disableTrigger(int triggerPointId) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    for (auto& point : m_triggerPoints) {
        if (point.id == triggerPointId) {
            point.enabled = false;
            return Result<void>::Success();
        }
    }

    return Result<void>::Failure(
        Error(ErrorCode::CONFIGURATION_ERROR, "Trigger point not found", "HardwareTestAdapter"));
}

Result<void> HardwareTestAdapter::clearAllTriggers() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    m_triggerPoints.clear();
    m_triggerEvents.clear();

    return Result<void>::Success();
}

std::vector<TriggerEvent> HardwareTestAdapter::getTriggerEvents() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_triggerEvents;
}

}  // namespace Siligen::Infrastructure::Adapters::Hardware



