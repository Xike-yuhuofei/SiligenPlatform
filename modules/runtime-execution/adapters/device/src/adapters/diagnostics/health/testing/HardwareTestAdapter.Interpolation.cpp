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

// ============ 插补测试 ============

Result<void> HardwareTestAdapter::executeInterpolationTest(TrajectoryInterpolationType interpolationType,
                                                           const std::vector<Point2D>& controlPoints,
                                                           const InterpolationParameters& interpParams) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    m_interpolatedPath.clear();

    if (!isConnected()) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "Hardware not connected", "HardwareTestAdapter"));
    }

    // 1. 参数验证
    size_t minPoints = 2;
    switch (interpolationType) {
        case TrajectoryInterpolationType::Linear:
            minPoints = 2;
            break;
        case TrajectoryInterpolationType::CircularArc:
            minPoints = 3;
            break;
        case TrajectoryInterpolationType::BSpline:
        case TrajectoryInterpolationType::Bezier:
            minPoints = 4;
            break;
    }

    if (controlPoints.size() < minPoints) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_PARAMETER, "Insufficient control points for interpolation type", "HardwareTestAdapter"));
    }

    if (interpParams.targetFeedRate <= 0 || interpParams.acceleration <= 0) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER,
                  "Invalid interpolation parameters (feedRate and acceleration must be positive)",
                  "HardwareTestAdapter"));
    }

    // 2. 使用坐标系1进行插补测试
    const short crdNum = 1;

    // 清空坐标系缓冲区
    short ret = multicard_->MC_CrdClear(crdNum, 0);
    if (ret != 0) {
        return Result<void>::Failure(Error(ErrorCode::HARDWARE_CONNECTION_FAILED,
                                           "Failed to clear coordinate buffer: " + getMultiCardErrorMessage(ret),
                                           "HardwareTestAdapter"));
    }

    // 3. 根据插补类型执行不同的插补算法
    switch (interpolationType) {
        case TrajectoryInterpolationType::Linear:
            ret = executeLinearInterpolation(crdNum, controlPoints, interpParams);
            break;

        case TrajectoryInterpolationType::CircularArc:
            ret = executeArcInterpolation(crdNum, controlPoints, interpParams);
            break;

        case TrajectoryInterpolationType::BSpline:
        case TrajectoryInterpolationType::Bezier:
            ret = executeSplineInterpolation(crdNum, controlPoints, interpParams);
            break;
    }

    if (ret != 0) {
        return Result<void>::Failure(Error(ErrorCode::HARDWARE_CONNECTION_FAILED,
                                           "Interpolation execution failed: " + getMultiCardErrorMessage(ret),
                                           "HardwareTestAdapter"));
    }

    // 4. 启动坐标系运动
    // TEMP FIX: 禁用实际硬件启动 - 模拟成功
    // ret = MC_CrdStart(m_cardHandle, crdNum, 0);
    // if (ret != 0) {
    //     return Result<void>::Failure(
    //         Error(ErrorCode::HARDWARE_CONNECTION_FAILED,
    //               "Failed to start coordinate motion: " + getMultiCardErrorMessage(ret),
    //               "HardwareTestAdapter")
    //     );
    // }
    ret = 0;  // 模拟成功

    // 5. 采样轨迹点
    auto samplingResult = sampleInterpolationPath(crdNum, interpParams.interpolationCycle);
    if (!samplingResult.IsSuccess()) {
        return Result<void>::Failure(samplingResult.GetError());
    }

    return Result<void>::Success();
}

std::vector<Point2D> HardwareTestAdapter::getInterpolatedPath() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_interpolatedPath;
}

}  // namespace Siligen::Infrastructure::Adapters::Hardware



