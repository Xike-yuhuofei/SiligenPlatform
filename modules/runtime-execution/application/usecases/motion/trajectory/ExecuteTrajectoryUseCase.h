#pragma once

#include "domain/dispensing/ports/ITriggerControllerPort.h"
#include "domain/motion/ports/IPositionControlPort.h"
#include "domain/supervision/ports/IEventPublisherPort.h"
#include "shared/types/Point.h"
#include "shared/types/Result.h"

#include <memory>
#include <string>
#include <vector>

namespace Siligen::Application::UseCases::Motion::Trajectory {

/**
 * @brief 轨迹段类型
 */
enum class TrajectorySegmentType {
    LINEAR,  // 直线
    ARC_CW,  // 顺时针圆弧
    ARC_CCW  // 逆时针圆弧
};

/**
 * @brief 轨迹段定义
 */
struct TrajectorySegment {
    TrajectorySegmentType type = TrajectorySegmentType::LINEAR;
    Point2D start_point{0, 0};
    Point2D end_point{0, 0};
    Point2D center_point{0, 0};  // 圆弧中心点(仅用于ARC)
    float32 velocity = 0.0f;
    float32 acceleration = 0.0f;
    bool enable_trigger = false;         // 是否启用CMP触发
    float32 trigger_interval_mm = 0.0f;  // 触发间隔(mm)
};

/**
 * @brief 轨迹执行请求
 */
struct ExecuteTrajectoryRequest {
    std::vector<TrajectorySegment> trajectory_segments;
    bool validate_trajectory = true;
    bool smooth_transition = true;  // 轨迹段之间平滑过渡
    bool wait_for_completion = true;
    int32 timeout_ms = 60000;

    bool Validate() const {
        if (trajectory_segments.empty()) {
            return false;
        }
        for (const auto& segment : trajectory_segments) {
            if (segment.velocity <= 0 || segment.velocity > 1000.0f) {
                return false;
            }
        }
        return timeout_ms > 0;
    }
};

/**
 * @brief 轨迹执行响应
 */
struct ExecuteTrajectoryResponse {
    int32 segments_executed = 0;
    int32 total_segments = 0;
    float32 total_distance_mm = 0.0f;
    int32 execution_time_ms = 0;
    int32 triggers_activated = 0;
    Point2D final_position{0, 0};
    bool completed_successfully = false;
    std::string status_message;
};

/**
 * @brief 轨迹执行用例
 *
 * 业务流程:
 * 1. 验证轨迹有效性
 * 2. 检查系统状态
 * 3. 配置CMP触发(如果需要)
 * 4. 逐段执行轨迹
 * 5. 监控执行状态
 * 6. 等待完成并验证
 */
class ExecuteTrajectoryUseCase {
   public:
    explicit ExecuteTrajectoryUseCase(std::shared_ptr<Domain::Motion::Ports::IPositionControlPort> position_control_port,
                                      std::shared_ptr<Domain::Dispensing::Ports::ITriggerControllerPort> trigger_port,
                                      std::shared_ptr<Domain::System::Ports::IEventPublisherPort> event_port);

    ~ExecuteTrajectoryUseCase() = default;

    ExecuteTrajectoryUseCase(const ExecuteTrajectoryUseCase&) = delete;
    ExecuteTrajectoryUseCase& operator=(const ExecuteTrajectoryUseCase&) = delete;
    ExecuteTrajectoryUseCase(ExecuteTrajectoryUseCase&&) = delete;
    ExecuteTrajectoryUseCase& operator=(ExecuteTrajectoryUseCase&&) = delete;

    /**
     * @brief 执行轨迹
     */
    Result<ExecuteTrajectoryResponse> Execute(const ExecuteTrajectoryRequest& request);

    /**
     * @brief 暂停轨迹执行
     */
    Result<void> Pause();

    /**
     * @brief 恢复轨迹执行
     */
    Result<void> Resume();

    /**
     * @brief 取消轨迹执行
     */
    Result<void> Cancel();

   private:
    std::shared_ptr<Domain::Motion::Ports::IPositionControlPort> position_control_port_;
    std::shared_ptr<Domain::Dispensing::Ports::ITriggerControllerPort> trigger_port_;
    std::shared_ptr<Domain::System::Ports::IEventPublisherPort> event_port_;

    bool is_paused_;
    bool is_cancelled_;

    // 执行步骤
    Result<void> ValidateTrajectory(const ExecuteTrajectoryRequest& request);
    Result<void> ExecuteSegment(const TrajectorySegment& segment,
                                ExecuteTrajectoryResponse& response,
                                int32 timeout_ms);
    Result<void> ExecuteLinearSegment(const TrajectorySegment& segment);
    Result<void> ExecuteArcSegment(const TrajectorySegment& segment, int32 timeout_ms);
    Result<void> ConfigureTriggerForSegment(const TrajectorySegment& segment);
    void PublishTrajectoryEvent(const std::string& event_type, const ExecuteTrajectoryResponse& response);
};

}  // namespace Siligen::Application::UseCases::Motion::Trajectory





