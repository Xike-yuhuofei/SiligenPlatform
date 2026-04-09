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

static bool calculateArcCenter(const Point2D& p1,
                               const Point2D& p2,
                               const Point2D& p3,
                               double& centerX,
                               double& centerY,
                               double& radius,
                               bool& clockwise);
static std::vector<Point2D> discretizeSpline(const std::vector<Point2D>& controlPoints, int segmentCount = 50);

// ============ 私有辅助方法 ============

short HardwareTestAdapter::executeLinearInterpolation(short crdNum,
                                                      const std::vector<Point2D>& controlPoints,
                                                      const InterpolationParameters& interpParams) {
    // 连接所有控制点形成折线
    for (size_t i = 1; i < controlPoints.size(); ++i) {
        const Point2D& target = controlPoints[i];

        // 转换为脉冲单位（假设1mm = 1000脉冲）
        long xPulse = static_cast<long>(target.x * 1000);
        long yPulse = static_cast<long>(target.y * 1000);

        short ret = multicard_->MC_LnXY(crdNum,
                                        xPulse,
                                        yPulse,
                                        interpParams.targetFeedRate,
                                        interpParams.acceleration,
                                        0.0,                  // endVel
                                        0,                    // fifo
                                        static_cast<long>(i)  // segmentId
        );

        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

short HardwareTestAdapter::executeArcInterpolation(short crdNum,
                                                   const std::vector<Point2D>& controlPoints,
                                                   const InterpolationParameters& interpParams) {
    // 从前三个点计算圆心
    if (controlPoints.size() < 3) {
        return -1;  // 错误：点数不足
    }

    const Point2D& p1 = controlPoints[0];
    const Point2D& p2 = controlPoints[1];
    const Point2D& p3 = controlPoints[2];

    double centerX, centerY, radius;
    bool clockwise;

    if (!calculateArcCenter(p1, p2, p3, centerX, centerY, radius, clockwise)) {
        return -2;  // 错误：三点共线
    }

    // 计算圆心相对于起点的偏移
    double xCenter = centerX - p1.x;
    double yCenter = centerY - p1.y;

    // 转换为脉冲单位
    long xPulse = static_cast<long>(p3.x * 1000);
    long yPulse = static_cast<long>(p3.y * 1000);

    short ret = multicard_->MC_ArcXYC(crdNum,
                                      xPulse,
                                      yPulse,
                                      xCenter,
                                      yCenter,
                                      clockwise ? 1 : 0,  // circleDir: 1=顺时针, 0=逆时针
                                      interpParams.targetFeedRate,
                                      interpParams.acceleration,
                                      0.0,  // endVel
                                      0,    // fifo
                                      1     // segmentId
    );

    return ret;
}

short HardwareTestAdapter::executeSplineInterpolation(short crdNum,
                                                      const std::vector<Point2D>& controlPoints,
                                                      const InterpolationParameters& interpParams) {
    // 将样条曲线离散化为直线段
    std::vector<Point2D> discretePoints = discretizeSpline(controlPoints, 50);

    if (discretePoints.empty()) {
        return -1;
    }

    // 使用直线插补连接离散点
    for (size_t i = 1; i < discretePoints.size(); ++i) {
        const Point2D& target = discretePoints[i];

        long xPulse = static_cast<long>(target.x * 1000);
        long yPulse = static_cast<long>(target.y * 1000);

        short ret = multicard_->MC_LnXY(crdNum,
                                        xPulse,
                                        yPulse,
                                        interpParams.targetFeedRate,
                                        interpParams.acceleration,
                                        0.0,
                                        0,
                                        static_cast<long>(i));

        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

Result<void> HardwareTestAdapter::sampleInterpolationPath(short crdNum, int sampleIntervalMs) {
    m_interpolatedPath.clear();

    // 持续采样直到运动完成
    const int maxSamples = 10000;  // 防止无限循环
    int sampleCount = 0;

    while (sampleCount < maxSamples) {
        // 检查坐标系状态
        short crdStatus = 0;
        long segment = 0;
        short ret = multicard_->MC_CrdStatus(crdNum, &crdStatus, &segment);

        if (ret != 0) {
            return Result<void>::Failure(Error(ErrorCode::HARDWARE_CONNECTION_FAILED,
                                               "Failed to get coordinate status: " + getMultiCardErrorMessage(ret),
                                               "HardwareTestAdapter"));
        }

        // 采样当前位置
        double pos[8] = {0};  // 最多8个轴
        // TEMP FIX: 禁用实际硬件采样 - 使用模拟数据
        // ret = MC_GetCrdPos(crdNum, pos);
        // if (ret != 0) {
        //     return Result<void>::Failure(
        //         Error(ErrorCode::HARDWARE_CONNECTION_FAILED,
        //               "Failed to get coordinate position: " + getMultiCardErrorMessage(ret),
        //               "HardwareTestAdapter")
        //     );
        // }

        // 模拟采样数据（用于调试）
        pos[0] = sampleCount * 100.0;  // 模拟X轴位置增长
        pos[1] = sampleCount * 50.0;   // 模拟Y轴位置增长
        ret = 0;                       // 模拟成功

        // 转换为mm单位并存储
        Point2D sample;
        sample.x = pos[0] / 1000.0;
        sample.y = pos[1] / 1000.0;
        // 注意：Point2D没有timestamp字段，使用vector索引作为时间序列
        m_interpolatedPath.push_back(sample);

        sampleCount++;

        // 检查是否运动完成 (bit0=0表示停止)
        if ((crdStatus & 0x01) == 0) {
            break;
        }

        // 等待采样间隔
        std::this_thread::sleep_for(std::chrono::milliseconds(sampleIntervalMs));
    }

    if (sampleCount >= maxSamples) {
        return Result<void>::Failure(
            Error(ErrorCode::MOTION_TIMEOUT, "Interpolation sampling timeout", "HardwareTestAdapter"));
    }

    return Result<void>::Success();
}

Result<void> HardwareTestAdapter::validateAxisNumber(int axis) const {
    if (axis < 0 || axis >= axis_count_) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Invalid axis number (must be 0-1)", "HardwareTestAdapter"));
    }

    return Result<void>::Success();
}

std::string HardwareTestAdapter::getMultiCardErrorMessage(short errorCode) const {
    switch (errorCode) {
        case MC_COM_SUCCESS:
            return "成功";
        case MC_COM_ERR_EXEC_FAIL:
            return "执行失败 - 检测命令执行条件是否满足";
        case MC_COM_ERR_LICENSE_WRONG:
            return "版本不支持该API";
        case MC_COM_ERR_DATA_WORRY:
            return "参数错误";
        case MC_COM_ERR_SEND:
            return "通讯失败";
        case MC_COM_ERR_CARD_OPEN_FAIL:
            return "打开控制器失败";
        case MC_COM_ERR_TIME_OUT:
            return "运动控制器无响应";
        case MC_COM_ERR_COM_OPEN_FAIL:
            return "串口打开失败";
        default:
            return "未知错误码: " + std::to_string(errorCode);
    }
}

// ============ 插补辅助函数 ============

/**
 * @brief 从三个点计算圆心和半径
 * @param p1 起点
 * @param p2 中间点
 * @param p3 终点
 * @param centerX 输出圆心X坐标
 * @param centerY 输出圆心Y坐标
 * @param radius 输出半径
 * @param clockwise 输出是否为顺时针方向
 * @return 是否成功计算（三点不共线）
 */
static bool calculateArcCenter(const Point2D& p1,
                               const Point2D& p2,
                               const Point2D& p3,
                               double& centerX,
                               double& centerY,
                               double& radius,
                               bool& clockwise) {
    // 使用三点外接圆公式
    double ax = p1.x, ay = p1.y;
    double bx = p2.x, by = p2.y;
    double cx = p3.x, cy = p3.y;

    double d = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));

    // 检查是否共线
    if (std::abs(d) < 1e-6) {
        return false;
    }

    double ux = ax * ax + ay * ay;
    double uy = bx * bx + by * by;
    double uz = cx * cx + cy * cy;

    centerX = (ux * (by - cy) + uy * (cy - ay) + uz * (ay - by)) / d;
    centerY = (ux * (cx - bx) + uy * (ax - cx) + uz * (bx - ax)) / d;

    radius = std::sqrt((ax - centerX) * (ax - centerX) + (ay - centerY) * (ay - centerY));

    // 判断方向：使用叉积
    double crossProduct = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    clockwise = (crossProduct < 0);

    return true;
}

/**
 * @brief 将样条曲线离散化为直线段
 * @param controlPoints 控制点
 * @param segmentCount 离散化段数
 * @return 离散化后的点序列
 */
static std::vector<Point2D> discretizeSpline(const std::vector<Point2D>& controlPoints, int segmentCount) {
    std::vector<Point2D> discretePoints;

    if (controlPoints.size() < 2) {
        return discretePoints;
    }

    // 使用线性插值作为简化实现
    // TODO: 实现真正的B样条或贝塞尔曲线算法
    for (int i = 0; i < segmentCount; ++i) {
        double t = static_cast<double>(i) / (segmentCount - 1);
        int segIndex = static_cast<int>(t * (controlPoints.size() - 1));
        if (segIndex >= static_cast<int>(controlPoints.size()) - 1) {
            segIndex = static_cast<int>(controlPoints.size()) - 2;
        }

        double localT = t * (controlPoints.size() - 1) - segIndex;
        const Point2D& p1 = controlPoints[segIndex];
        const Point2D& p2 = controlPoints[segIndex + 1];

        Point2D interpolated;
        interpolated.x = p1.x + (p2.x - p1.x) * localT;
        interpolated.y = p1.y + (p2.y - p1.y) * localT;
        discretePoints.push_back(interpolated);
    }

    return discretePoints;
}

}  // namespace Siligen::Infrastructure::Adapters::Hardware



