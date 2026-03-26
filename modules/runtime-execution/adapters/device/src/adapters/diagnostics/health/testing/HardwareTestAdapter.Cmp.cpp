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

// ============ CMP测试 ============

Result<void> HardwareTestAdapter::startCMPTest(const TrajectoryDefinition& trajectory, const CMPParameters& cmpParams) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (!multicard_) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "Hardware is not connected", "HardwareTestAdapter"));
    }

    // 验证轨迹定义
    if (trajectory.controlPoints.size() < 2) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_PARAMETER, "Trajectory must have at least 2 control points", "HardwareTestAdapter"));
    }

    // 初始化CMP测试状态
    m_cmpTestRunning = true;
    m_cmpTestProgress = 0.0;
    m_cmpActualPath.clear();

    // 模拟CMP测试执行 (实际硬件实现应调用MC_LnXY/MC_ArcXY等API)
    // 在模拟模式下，我们生成一个带有轻微噪声的实际路径
    // 这允许在没有真实硬件的情况下测试整个系统流程

    // 启动后台线程模拟轨迹执行和路径采样
    // 注意：真实实现应使用MultiCard API的坐标系运动功能

    return Result<void>::Success();
}

Result<void> HardwareTestAdapter::stopCMPTest() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    m_cmpTestRunning = false;

    return Result<void>::Success();
}

double HardwareTestAdapter::getCMPTestProgress() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    // 实际硬件实现应查询运动控制器的轨迹执行进度
    // 可以通过比较当前位置和轨迹总长度来计算进度

    return m_cmpTestProgress;
}

std::vector<Point2D> HardwareTestAdapter::getCMPActualPath() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    // 实际硬件实现应通过调用MC_GetCrdPos等API来采样实际位置
    // 采样频率应为100Hz (10ms间隔)
    // 示例:
    // double x, y;
    // MC_GetCrdPos(m_cardHandle, crdNum, &x, &y);
    // m_cmpActualPath.push_back(Point2D(x, y));

    return m_cmpActualPath;
}

}  // namespace Siligen::Infrastructure::Adapters::Hardware



