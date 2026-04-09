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

// ============ 连接管理 ============

bool HardwareTestAdapter::isConnected() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    // Step 1: 检查指针是否有效
    if (!multicard_) {
        return false;
    }

    // Step 2: 实时硬件检测 - 尝试读取轴1状态验证硬件响应
    // 这是唯一可靠的方法来确认控制卡是否真正通电
    long status = 0;
    unsigned long clock = 0;
    short ret = multicard_->MC_GetSts(1, &status, 1, &clock);

    // Step 3: 返回检测结果
    // ret == 0 表示硬件正常响应
    // ret != 0 表示硬件未响应（未通电、断线等）
    return (ret == 0);
}

std::map<LogicalAxisId, bool> HardwareTestAdapter::getAxisEnableStates() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    std::map<LogicalAxisId, bool> states;
    if (!multicard_) return states;

    for (int axis = 0; axis < axis_count_; ++axis) {
        auto axis_id = FromIndex(static_cast<int16>(axis));
        if (!IsValid(axis_id)) {
            continue;
        }
        long status = 0;
        unsigned long clock = 0;
        // MultiCard API使用1-based轴号
        short ret = multicard_->MC_GetSts(static_cast<short>(axis + 1), &status, 1, &clock);

        if (ret == 0) {
            states[axis_id] = (status & AXIS_STATUS_ENABLE) != 0;
        } else {
            states[axis_id] = false;
        }
    }

    return states;
}

}  // namespace Siligen::Infrastructure::Adapters::Hardware



