#pragma once

#include "process_planning/contracts/configuration/IConfigurationPort.h"
#include "runtime_execution/contracts/motion/IHomingPort.h"
#include "runtime_execution/contracts/motion/IMotionConnectionPort.h"
#include "runtime_execution/contracts/motion/IMotionStatePort.h"
#include "shared/types/AxisTypes.h"
#include "shared/types/Result.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace Siligen::Domain::Motion::DomainServices {

using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Result;

/**
 * @brief 回零请求
 */
struct HomeAxesRequest {
    std::vector<LogicalAxisId> axes;  // 需要回零的轴列表,空列表表示所有轴
    bool home_all_axes = false;
    bool force_rehome = false;  // 强制重新回零(忽略已回零状态)
    bool wait_for_completion = true;
    int32 timeout_ms = 80000;  // 每个轴的超时时间

    bool Validate() const {
        if (home_all_axes && !axes.empty()) {
            return false;  // 不能同时指定home_all和axes列表
        }
        return timeout_ms > 0 && timeout_ms <= 300000;
    }
};

/**
 * @brief 回零响应
 */
struct HomeAxesResponse {
    struct AxisResult {
        LogicalAxisId axis = LogicalAxisId::INVALID;
        bool success = false;
        std::string message;
    };

    std::vector<LogicalAxisId> successfully_homed_axes;
    std::vector<LogicalAxisId> failed_axes;
    std::vector<std::string> error_messages;
    std::vector<AxisResult> axis_results;
    int32 total_time_ms = 0;
    bool all_completed = false;
    std::string status_message;
};

/**
 * @brief 回零流程与规则
 *
 * 业务流程:
 * 1. 验证请求参数
 * 2. 检查系统状态是否允许回零
 * 3. 使用已加载配置（轴数量/范围）
 * 4. 执行回零操作
 * 5. 等待回零完成(可选)
 * 6. 验证回零状态
 * 7. 记录回零结果
 */
class HomingProcess final {
   public:
    explicit HomingProcess(std::shared_ptr<Ports::IHomingPort> homing_port,
                           std::shared_ptr<Configuration::Ports::IConfigurationPort> config_port,
                           std::shared_ptr<Ports::IMotionConnectionPort> motion_connection_port,
                           std::shared_ptr<Ports::IMotionStatePort> motion_state_port);

    HomingProcess(const HomingProcess&) = delete;
    HomingProcess& operator=(const HomingProcess&) = delete;
    HomingProcess(HomingProcess&&) = delete;
    HomingProcess& operator=(HomingProcess&&) = delete;

    Result<HomeAxesResponse> Execute(const HomeAxesRequest& request);
    Result<void> StopHoming();
    Result<void> StopHomingAxes(const std::vector<LogicalAxisId>& axes);
    Result<bool> IsAxisHomed(LogicalAxisId axis_id) const;

   private:
    std::shared_ptr<Ports::IHomingPort> homing_port_;
    std::shared_ptr<Configuration::Ports::IConfigurationPort> config_port_;
    std::shared_ptr<Ports::IMotionConnectionPort> motion_connection_port_;
    std::shared_ptr<Ports::IMotionStatePort> motion_state_port_;

    Result<void> ValidateRequest(const HomeAxesRequest& request);
    Result<void> CheckSystemState();
    Result<void> ValidateHomingConfiguration(const std::vector<LogicalAxisId>& axes, int axis_count) const;
    Result<void> ValidateSafetyPreconditions(const std::vector<LogicalAxisId>& axes) const;
    Result<void> HomeAxis(LogicalAxisId axis_id);
    Result<void> WaitForHomingComplete(LogicalAxisId axis_id, int32 timeout_ms);
    Result<void> WaitForAxisSettleAfterHoming(
        LogicalAxisId axis_id,
        const std::chrono::steady_clock::time_point& deadline) const;
    Result<int32> GetHomingSettleTimeMs(LogicalAxisId axis_id) const;
    Result<void> VerifyHomingSuccess(LogicalAxisId axis_id);
    Result<int> GetConfiguredAxisCount() const;
};

}  // namespace Siligen::Domain::Motion::DomainServices
