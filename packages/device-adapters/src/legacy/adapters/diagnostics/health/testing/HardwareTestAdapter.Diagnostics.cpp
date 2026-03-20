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

// ============ 系统诊断 ============

HardwareCheckResult HardwareTestAdapter::checkHardwareConnection() {
    HardwareCheckResult result;

    result.controllerConnected = isConnected();

    if (result.controllerConnected) {
        auto enableStates = getAxisEnableStates();
        for (const auto& [axis_id, enabled] : enableStates) {
            if (enabled) {
                result.enabledAxes.push_back(axis_id);
            }
        }

        // 检查限位开关和编码器状态
        for (int axis = 0; axis < axis_count_; ++axis) {
            auto axis_id = FromIndex(static_cast<int16>(axis));
            if (!IsValid(axis_id)) {
                continue;
            }
            auto limitState = getLimitSwitchState(axis_id);
            result.limitSwitchOk[axis_id] = !limitState.positiveLimitTriggered && !limitState.negativeLimitTriggered;
            result.encoderOk[axis_id] = true;  // TODO: 实际检查编码器状态
        }
    }

    return result;
}

CommunicationCheckResult HardwareTestAdapter::testCommunicationQuality(int testDurationMs) {
    CommunicationCheckResult result;

    if (testDurationMs <= 0) {
        return result;  // 无效测试时长
    }

    auto startTime = std::chrono::steady_clock::now();
    auto endTime = startTime + std::chrono::milliseconds(testDurationMs);

    std::vector<double> responseTimes;
    int totalCommands = 0;
    int failedCommands = 0;

    // 持续发送测试命令直到时间耗尽
    while (std::chrono::steady_clock::now() < endTime) {
        totalCommands++;

        auto cmdStart = std::chrono::steady_clock::now();

        // 发送简单的状态查询命令测试通信质量
        long status = 0;
        unsigned long clock = 0;
        short ret = multicard_->MC_GetSts(0, &status, 1, &clock);

        auto cmdEnd = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(cmdEnd - cmdStart);
        double responseTimeMs = duration.count() / 1000.0;

        if (ret != 0) {
            failedCommands++;
        } else {
            responseTimes.push_back(responseTimeMs);
        }

        // 避免过于频繁的查询
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    result.totalCommands = totalCommands;
    result.failedCommands = failedCommands;

    if (!responseTimes.empty()) {
        result.avgResponseTimeMs =
            std::accumulate(responseTimes.begin(), responseTimes.end(), 0.0) / responseTimes.size();
        result.maxResponseTimeMs = *std::max_element(responseTimes.begin(), responseTimes.end());
    }

    if (totalCommands > 0) {
        result.packetLossRate = 100.0 * failedCommands / totalCommands;
    }

    return result;
}

Result<double> HardwareTestAdapter::measureAxisResponseTime(LogicalAxisId axis_id) {
    const int axis = AxisIndex(axis_id);
    auto validateResult = validateAxisNumber(axis);
    if (!validateResult.IsSuccess()) {
        return Result<double>::Failure(validateResult.GetError());
    }

    // 发送简单的速度设置命令并测量响应时间
    auto cmdStart = std::chrono::steady_clock::now();

    // MultiCard API使用1-based轴号
    // 注意：这里的10.0是pulse/ms单位，用于测试响应时间
    short ret = multicard_->MC_SetVel(static_cast<short>(axis + 1), 10.0);

    auto cmdEnd = std::chrono::steady_clock::now();

    if (ret != 0) {
        return Result<double>::Failure(Error(ErrorCode::HARDWARE_CONNECTION_FAILED,
                                             "响应时间测量失败: " + getMultiCardErrorMessage(ret),
                                             "HardwareTestAdapter"));
    }

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(cmdEnd - cmdStart);
    double responseTimeMs = duration.count() / 1000.0;

    return Result<double>::Success(responseTimeMs);
}

Result<AccuracyCheckResult> HardwareTestAdapter::testPositioningAccuracy(LogicalAxisId axis_id, int testCycles) {
    const int axis = AxisIndex(axis_id);
    auto validateResult = validateAxisNumber(axis);
    if (!validateResult.IsSuccess()) {
        return Result<AccuracyCheckResult>::Failure(validateResult.GetError());
    }

    if (testCycles <= 0) {
        return Result<AccuracyCheckResult>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "测试循环次数必须大于0", "HardwareTestAdapter"));
    }

    AccuracyCheckResult result;

    // 获取当前位置作为起点
    double startPos = 0.0;
    // MultiCard API使用1-based轴号
    short ret = multicard_->MC_GetPrfPos(static_cast<short>(axis + 1), &startPos);
    if (ret != 0) {
        return Result<AccuracyCheckResult>::Failure(Error(ErrorCode::HARDWARE_CONNECTION_FAILED,
                                                          "获取轴位置失败: " + getMultiCardErrorMessage(ret),
                                                          "HardwareTestAdapter"));
    }

    // 定义测试目标位置（往返100mm）
    double testDistance = 100.0;
    double targetPos = startPos + testDistance;

    std::vector<double> forwardPositions;  // 正向到达位置
    std::vector<double> returnPositions;   // 返回到达位置

    // 执行往返测试
    for (int cycle = 0; cycle < testCycles; ++cycle) {
        // 正向移动
        // TODO: 实际应调用点到点移动并等待完成，这里简化处理
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        double actualPos = 0.0;
        // MultiCard API使用1-based轴号
        ret = multicard_->MC_GetPrfPos(static_cast<short>(axis + 1), &actualPos);
        if (ret == 0) {
            forwardPositions.push_back(actualPos);
        }

        // 反向移动回起点
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // MultiCard API使用1-based轴号
        ret = multicard_->MC_GetPrfPos(static_cast<short>(axis + 1), &actualPos);
        if (ret == 0) {
            returnPositions.push_back(actualPos);
        }
    }

    // 计算重复定位精度（返回位置的最大偏差）
    if (!returnPositions.empty()) {
        double maxDev = 0.0;
        for (double pos : returnPositions) {
            double deviation = std::abs(pos - startPos);
            if (deviation > maxDev) {
                maxDev = deviation;
            }
        }
        result.repeatabilityError[axis_id] = maxDev;
    }

    // 计算单向定位精度（正向位置的最大偏差）
    if (!forwardPositions.empty()) {
        double maxDev = 0.0;
        for (double pos : forwardPositions) {
            double deviation = std::abs(pos - targetPos);
            if (deviation > maxDev) {
                maxDev = deviation;
            }
        }
        result.positioningError[axis_id] = maxDev;
    }

    // 判断是否满足精度要求（默认0.05mm）
    double maxError = std::max(result.repeatabilityError[axis_id], result.positioningError[axis_id]);
    result.meetsAccuracyRequirement = (maxError <= result.requiredAccuracy);

    return Result<AccuracyCheckResult>::Success(result);
}

}  // namespace Siligen::Infrastructure::Adapters::Hardware



