#include "HardwareTestAdapter.h"

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

// ============ I/O测试 ============

Result<void> HardwareTestAdapter::setDigitalOutput(int port, bool value) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    short ret = multicard_->MC_SetExtDoBit(0, static_cast<short>(port), value ? 1 : 0);

    if (ret != 0) {
        return Result<void>::Failure(Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                                           "Failed to set digital output: " + getMultiCardErrorMessage(ret),
                                           "HardwareTestAdapter"));
    }

    return Result<void>::Success();
}

Result<bool> HardwareTestAdapter::getDigitalInput(int port) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    long diValue = 0;
    short ret = multicard_->MC_GetDiRaw(0, &diValue);

    if (ret != 0) {
        return Result<bool>::Failure(Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                                           "Failed to get digital input: " + getMultiCardErrorMessage(ret),
                                           "HardwareTestAdapter"));
    }

    bool value = (diValue & (1 << port)) != 0;
    return Result<bool>::Success(value);
}

std::map<int, bool> HardwareTestAdapter::getAllDigitalInputs() const {
    std::map<int, bool> inputs;

    for (int port = 0; port < 16; ++port) {
        auto result = getDigitalInput(port);
        if (result.IsSuccess()) {
            inputs[port] = result.Value();
        }
    }

    return inputs;
}

std::map<int, bool> HardwareTestAdapter::getAllDigitalOutputs() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::map<int, bool> outputs;

    unsigned long doValue = 0;
    short ret = multicard_->MC_GetExtDoValue(0, &doValue);

    if (ret == 0) {
        // 读取16个数字输出端口状态
        for (int port = 0; port < 16; ++port) {
            bool value = (doValue & (1 << port)) != 0;
            outputs[port] = value;
        }
    }

    return outputs;
}

}  // namespace Siligen::Infrastructure::Adapters::Hardware



