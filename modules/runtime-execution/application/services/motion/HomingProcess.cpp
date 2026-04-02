#include "runtime_execution/application/services/motion/HomingProcess.h"

#include "domain/safety/bridges/MotionCoreInterlockBridge.h"
#include "shared/types/Error.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <sstream>
#include <thread>
#include <utility>

namespace Siligen::RuntimeExecution::Application::Services::Motion {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::FromIndex;
using Siligen::Shared::Types::ToIndex;
using Siligen::Domain::Configuration::Ports::IConfigurationPort;
using Siligen::Domain::Motion::Ports::HomingState;
using Siligen::Domain::Motion::Ports::IHomingPort;
using Siligen::Domain::Motion::Ports::IMotionConnectionPort;
using Siligen::Domain::Motion::Ports::IMotionStatePort;

namespace {

constexpr const char* kErrorSource = "HomeAxesUseCase";
constexpr int32 kPostHomingPollIntervalMs = 20;
constexpr float kSettledVelocityThresholdMmS = 0.1f;

void AppendAxisResult(HomeAxesResponse& response,
                      LogicalAxisId axis_id,
                      bool success,
                      std::string message = {}) {
    HomeAxesResponse::AxisResult axis_result;
    axis_result.axis = axis_id;
    axis_result.success = success;
    axis_result.message = std::move(message);
    response.axis_results.push_back(std::move(axis_result));
}

std::string FormatAxisMessage(LogicalAxisId axis_id, std::string message) {
    if (message.empty()) {
        message = "Homing failed";
    }
    return std::string(Siligen::Shared::Types::AxisName(axis_id)) + ": " + message;
}

}  // namespace

HomingProcess::HomingProcess(std::shared_ptr<IHomingPort> homing_port,
                             std::shared_ptr<IConfigurationPort> config_port,
                             std::shared_ptr<IMotionConnectionPort> motion_connection_port,
                             std::shared_ptr<IMotionStatePort> motion_state_port)
    : homing_port_(std::move(homing_port)),
      config_port_(std::move(config_port)),
      motion_connection_port_(std::move(motion_connection_port)),
      motion_state_port_(std::move(motion_state_port)) {}

Result<HomeAxesResponse> HomingProcess::Execute(const HomeAxesRequest& request) {
    auto validation = ValidateRequest(request);
    if (validation.IsError()) {
        return Result<HomeAxesResponse>::Failure(validation.GetError());
    }

    auto state_check = CheckSystemState();
    if (state_check.IsError()) {
        return Result<HomeAxesResponse>::Failure(state_check.GetError());
    }

    auto axis_count_result = GetConfiguredAxisCount();
    if (axis_count_result.IsError()) {
        return Result<HomeAxesResponse>::Failure(axis_count_result.GetError());
    }
    const int axis_count = axis_count_result.Value();

    std::vector<LogicalAxisId> axes = request.axes;
    if (request.home_all_axes || axes.empty()) {
        axes.clear();
        for (int axis = 0; axis < axis_count; ++axis) {
            auto axis_id = FromIndex(static_cast<int16>(axis));
            if (Siligen::Shared::Types::IsValid(axis_id)) {
                axes.push_back(axis_id);
            }
        }
    }

    auto config_check = ValidateHomingConfiguration(axes, axis_count);
    if (config_check.IsError()) {
        return Result<HomeAxesResponse>::Failure(config_check.GetError());
    }

    auto safety_check = ValidateSafetyPreconditions(axes);
    if (safety_check.IsError()) {
        return Result<HomeAxesResponse>::Failure(safety_check.GetError());
    }

    HomeAxesResponse response;
    const auto start_time = std::chrono::steady_clock::now();

    const bool wait_for_completion = request.wait_for_completion;
    const bool force_rehome = request.force_rehome;
    std::vector<LogicalAxisId> started_axes;
    started_axes.reserve(axes.size());

    auto get_retry_count = [&](LogicalAxisId axis_id) -> int {
        if (!config_port_) {
            return 0;
        }
        const int axis_index = static_cast<int>(ToIndex(axis_id));
        auto config_result = config_port_->GetHomingConfig(axis_index);
        if (config_result.IsError()) {
            return 0;
        }
        return std::max(0, config_result.Value().retry_count);
    };

    std::vector<LogicalAxisId> axes_to_wait;
    axes_to_wait.reserve(axes.size());

    for (const auto axis_id : axes) {
        if (!force_rehome) {
            auto homed_result = IsAxisHomed(axis_id);
            if (homed_result.IsError()) {
                response.failed_axes.push_back(axis_id);
                const auto axis_message = FormatAxisMessage(axis_id, homed_result.GetError().GetMessage());
                response.error_messages.push_back(axis_message);
                AppendAxisResult(response, axis_id, false, homed_result.GetError().GetMessage());
                continue;
            }
            if (homed_result.Value()) {
                response.successfully_homed_axes.push_back(axis_id);
                AppendAxisResult(response, axis_id, true, "Already homed");
                continue;
            }
        }

        bool axis_in_progress = false;
        if (homing_port_) {
            auto in_progress_result = homing_port_->IsHomingInProgress(axis_id);
            if (in_progress_result.IsError()) {
                response.failed_axes.push_back(axis_id);
                const auto axis_message = FormatAxisMessage(axis_id, in_progress_result.GetError().GetMessage());
                response.error_messages.push_back(axis_message);
                AppendAxisResult(response, axis_id, false, in_progress_result.GetError().GetMessage());
                continue;
            }
            axis_in_progress = in_progress_result.Value();
        }

        if (!axis_in_progress) {
            auto homing_result = HomeAxis(axis_id);
            if (homing_result.IsError()) {
                response.failed_axes.push_back(axis_id);
                const auto axis_message = FormatAxisMessage(axis_id, homing_result.GetError().GetMessage());
                response.error_messages.push_back(axis_message);
                AppendAxisResult(response, axis_id, false, homing_result.GetError().GetMessage());
                continue;
            }
        }

        started_axes.push_back(axis_id);
        if (!wait_for_completion) {
            AppendAxisResult(response, axis_id, true, axis_in_progress ? "Homing in progress" : "Homing started");
            continue;
        }

        axes_to_wait.push_back(axis_id);
    }

    for (const auto axis_id : axes_to_wait) {
        const int retry_count = get_retry_count(axis_id);
        int retries_left = retry_count;
        bool homed = false;
        std::string last_error_message;
        auto abort_homing = [&]() {
            if (!homing_port_) {
                return;
            }
            (void)homing_port_->StopHoming(axis_id);
            (void)homing_port_->ResetHomingState(axis_id);
        };

        while (true) {
            auto wait_result = WaitForHomingComplete(axis_id, request.timeout_ms);
            if (wait_result.IsError()) {
                const bool can_recover_from_terminal_status =
                    wait_result.GetError().GetModule() != kErrorSource;
                if (can_recover_from_terminal_status) {
                    auto verify_result = VerifyHomingSuccess(axis_id);
                    if (verify_result.IsSuccess()) {
                        homed = true;
                        break;
                    }
                }
                last_error_message = wait_result.GetError().GetMessage();
            } else {
                auto verify_result = VerifyHomingSuccess(axis_id);
                if (verify_result.IsSuccess()) {
                    homed = true;
                    break;
                }
                last_error_message = verify_result.GetError().GetMessage();
            }

            if (retries_left <= 0) {
                abort_homing();
                break;
            }

            --retries_left;
            abort_homing();
            auto restart_result = HomeAxis(axis_id);
            if (restart_result.IsError()) {
                abort_homing();
                last_error_message = restart_result.GetError().GetMessage();
                break;
            }
        }

        if (homed) {
            response.successfully_homed_axes.push_back(axis_id);
            AppendAxisResult(response, axis_id, true, "Homing completed");
        } else {
            response.failed_axes.push_back(axis_id);
            if (last_error_message.empty()) {
                last_error_message = "Homing failed";
            }
            response.error_messages.push_back(FormatAxisMessage(axis_id, last_error_message));
            AppendAxisResult(response, axis_id, false, last_error_message);
        }
    }

    response.total_time_ms = static_cast<int32>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count());
    const bool all_already_homed = started_axes.empty() && response.failed_axes.empty();
    if (wait_for_completion) {
        response.all_completed = response.failed_axes.empty();
        if (all_already_homed) {
            response.status_message = "Already homed";
        } else {
            response.status_message = response.all_completed ? "Homing completed" : "Homing completed with errors";
        }
    } else {
        response.all_completed = false;
        if (all_already_homed) {
            response.status_message = "Already homed";
        } else {
            response.status_message = response.failed_axes.empty() ? "Homing started" : "Homing started with errors";
        }
    }

    return Result<HomeAxesResponse>::Success(response);
}

Result<void> HomingProcess::StopHoming() {
    if (!homing_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Homing port not initialized", kErrorSource));
    }

    auto axis_count_result = GetConfiguredAxisCount();
    if (axis_count_result.IsError()) {
        return Result<void>::Failure(axis_count_result.GetError());
    }
    const int axis_count = axis_count_result.Value();
    for (int axis = 0; axis < axis_count; ++axis) {
        auto axis_id = FromIndex(static_cast<int16>(axis));
        if (!Siligen::Shared::Types::IsValid(axis_id)) {
            continue;
        }
        auto result = homing_port_->StopHoming(axis_id);
        if (result.IsError()) {
            return result;
        }
    }

    return Result<void>::Success();
}

Result<void> HomingProcess::StopHomingAxes(const std::vector<LogicalAxisId>& axes) {
    if (axes.empty()) {
        return StopHoming();
    }

    if (!homing_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Homing port not initialized", kErrorSource));
    }

    auto axis_count_result = GetConfiguredAxisCount();
    if (axis_count_result.IsError()) {
        return Result<void>::Failure(axis_count_result.GetError());
    }
    const int axis_count = axis_count_result.Value();

    for (auto axis_id : axes) {
        if (!Siligen::Shared::Types::IsValid(axis_id)) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "Axis out of range", kErrorSource));
        }
        const int axis_index = static_cast<int>(ToIndex(axis_id));
        if (axis_index < 0 || axis_index >= axis_count) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "Axis out of range", kErrorSource));
        }
        auto result = homing_port_->StopHoming(axis_id);
        if (result.IsError()) {
            return result;
        }
    }

    return Result<void>::Success();
}

Result<bool> HomingProcess::IsAxisHomed(LogicalAxisId axis_id) const {
    if (!homing_port_) {
        return Result<bool>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Homing port not initialized", kErrorSource));
    }

    return homing_port_->IsAxisHomed(axis_id);
}

Result<void> HomingProcess::ValidateRequest(const HomeAxesRequest& request) {
    if (!request.Validate()) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Invalid homing request", kErrorSource));
    }
    auto axis_count_result = GetConfiguredAxisCount();
    if (axis_count_result.IsError()) {
        return Result<void>::Failure(axis_count_result.GetError());
    }
    const int axis_count = axis_count_result.Value();
    for (auto axis_id : request.axes) {
        if (!Siligen::Shared::Types::IsValid(axis_id)) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "Axis out of range", kErrorSource));
        }
        const int axis_index = static_cast<int>(ToIndex(axis_id));
        if (axis_index < 0 || axis_index >= axis_count) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "Axis out of range", kErrorSource));
        }
    }
    return Result<void>::Success();
}

Result<void> HomingProcess::CheckSystemState() {
    if (!motion_connection_port_) {
        return Result<void>::Success();
    }

    auto connected = motion_connection_port_->IsConnected();
    if (connected.IsError()) {
        return Result<void>::Failure(connected.GetError());
    }

    if (!connected.Value()) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "Hardware not connected", kErrorSource));
    }

    return Result<void>::Success();
}

Result<void> HomingProcess::ValidateHomingConfiguration(
    const std::vector<LogicalAxisId>& axes,
    int axis_count) const {
    if (!config_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Config port not initialized", kErrorSource));
    }

    auto validation_result = config_port_->ValidateConfiguration();
    if (validation_result.IsError()) {
        return Result<void>::Failure(validation_result.GetError());
    }
    if (!validation_result.Value()) {
        std::ostringstream oss;
        oss << "配置验证失败";
        auto errors_result = config_port_->GetValidationErrors();
        if (errors_result.IsSuccess() && !errors_result.Value().empty()) {
            oss << ": ";
            const auto& errors = errors_result.Value();
            for (size_t i = 0; i < errors.size(); ++i) {
                if (i > 0) {
                    oss << "; ";
                }
                oss << errors[i];
            }
        }
        return Result<void>::Failure(Error(ErrorCode::INVALID_CONFIG_VALUE, oss.str(), kErrorSource));
    }

    auto homing_configs_result = config_port_->GetAllHomingConfigs();
    if (homing_configs_result.IsError()) {
        return Result<void>::Failure(homing_configs_result.GetError());
    }

    const auto& homing_configs = homing_configs_result.Value();
    if (axis_count <= 0) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_CONFIG_VALUE, "num_axes must be positive", kErrorSource));
    }
    if (static_cast<int>(homing_configs.size()) < axis_count) {
        return Result<void>::Failure(
            Error(
                ErrorCode::INVALID_CONFIG_VALUE,
                "回零配置数量不足: num_axes=" + std::to_string(axis_count) +
                    ", homing_configs=" + std::to_string(homing_configs.size()),
                kErrorSource));
    }

    for (auto axis_id : axes) {
        if (!Siligen::Shared::Types::IsValid(axis_id)) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "Axis out of range", kErrorSource));
        }
        const int axis_index = static_cast<int>(ToIndex(axis_id));
        if (axis_index < 0 || axis_index >= axis_count || axis_index >= static_cast<int>(homing_configs.size())) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "Axis out of range", kErrorSource));
        }
        const auto& config = homing_configs[static_cast<size_t>(axis_index)];
        if (config.axis != axis_index) {
            return Result<void>::Failure(
                Error(
                    ErrorCode::INVALID_CONFIG_VALUE,
                    "回零配置轴号不匹配: axis=" + std::to_string(axis_index) +
                        ", config.axis=" + std::to_string(config.axis),
                    kErrorSource));
        }
    }

    return Result<void>::Success();
}

Result<void> HomingProcess::ValidateSafetyPreconditions(const std::vector<LogicalAxisId>& axes) const {
    if (!motion_state_port_) {
        return Result<void>::Success();
    }

    for (auto axis_id : axes) {
        auto status_result = motion_state_port_->GetAxisStatus(axis_id);
        if (status_result.IsError()) {
            return Result<void>::Failure(status_result.GetError());
        }

        const auto& status = status_result.Value();
        const std::string axis_name = Siligen::Shared::Types::AxisName(axis_id);
        auto interlock_result =
            Siligen::Domain::Safety::Bridges::CheckAxisForHomingWithMotionCore(status, axis_name.c_str(), kErrorSource);
        if (interlock_result.IsError()) {
            return Result<void>::Failure(interlock_result.GetError());
        }
    }

    return Result<void>::Success();
}

Result<void> HomingProcess::HomeAxis(LogicalAxisId axis_id) {
    if (!homing_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Homing port not initialized", kErrorSource));
    }

    return homing_port_->HomeAxis(axis_id);
}

Result<void> HomingProcess::WaitForHomingComplete(LogicalAxisId axis_id, int32 timeout_ms) {
    if (!homing_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Homing port not initialized", kErrorSource));
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    auto wait_result = homing_port_->WaitForHomingComplete(axis_id, timeout_ms);
    if (wait_result.IsError()) {
        return wait_result;
    }

    return WaitForAxisSettleAfterHoming(axis_id, deadline);
}

Result<void> HomingProcess::WaitForAxisSettleAfterHoming(
    LogicalAxisId axis_id,
    const std::chrono::steady_clock::time_point& deadline) const {
    if (!motion_state_port_) {
        return Result<void>::Success();
    }

    auto settle_time_result = GetHomingSettleTimeMs(axis_id);
    if (settle_time_result.IsError()) {
        return Result<void>::Failure(settle_time_result.GetError());
    }

    const auto settle_time_ms = settle_time_result.Value();
    if (settle_time_ms <= 0) {
        return Result<void>::Success();
    }

    std::chrono::steady_clock::time_point stable_since{};
    while (true) {
        auto velocity_result = motion_state_port_->GetAxisVelocity(axis_id);
        if (velocity_result.IsError()) {
            return Result<void>::Failure(velocity_result.GetError());
        }

        const auto velocity = std::fabs(velocity_result.Value());
        const auto now = std::chrono::steady_clock::now();
        if (velocity <= kSettledVelocityThresholdMmS) {
            if (stable_since == std::chrono::steady_clock::time_point{}) {
                stable_since = now;
            }

            const auto stable_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(now - stable_since).count();
            if (stable_ms >= settle_time_ms) {
                return Result<void>::Success();
            }
        } else {
            stable_since = std::chrono::steady_clock::time_point{};
        }

        if (now >= deadline) {
            return Result<void>::Failure(
                Error(
                    ErrorCode::TIMEOUT,
                    "Axis " + std::to_string(ToIndex(axis_id)) +
                        " did not settle after homing: velocity=" + std::to_string(velocity) + " mm/s",
                    kErrorSource));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(kPostHomingPollIntervalMs));
    }
}

Result<int32> HomingProcess::GetHomingSettleTimeMs(LogicalAxisId axis_id) const {
    if (!config_port_) {
        return Result<int32>::Success(0);
    }

    auto config_result = config_port_->GetHomingConfig(static_cast<int>(ToIndex(axis_id)));
    if (config_result.IsError()) {
        return Result<int32>::Failure(config_result.GetError());
    }

    return Result<int32>::Success(std::max<int32>(0, config_result.Value().settle_time_ms));
}

Result<void> HomingProcess::VerifyHomingSuccess(LogicalAxisId axis_id) {
    auto status_result = homing_port_->GetHomingStatus(axis_id);
    if (status_result.IsError()) {
        return Result<void>::Failure(status_result.GetError());
    }

    if (status_result.Value().state == HomingState::HOMED) {
        return Result<void>::Success();
    }

    return Result<void>::Failure(
        Error(
            ErrorCode::HARDWARE_ERROR,
            "Axis " + std::to_string(ToIndex(axis_id)) + " homing not completed",
            kErrorSource));
}

Result<int> HomingProcess::GetConfiguredAxisCount() const {
    if (!config_port_) {
        return Result<int>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Config port not initialized", kErrorSource));
    }

    auto config_result = config_port_->GetHardwareConfiguration();
    if (config_result.IsError()) {
        return Result<int>::Failure(config_result.GetError());
    }

    int axis_count = config_result.Value().num_axes;
    if (axis_count <= 0) {
        return Result<int>::Failure(
            Error(ErrorCode::INVALID_CONFIG_VALUE, "num_axes must be positive", kErrorSource));
    }
    axis_count = static_cast<int>(config_result.Value().EffectiveAxisCount());
    return Result<int>::Success(axis_count);
}

}  // namespace Siligen::RuntimeExecution::Application::Services::Motion
