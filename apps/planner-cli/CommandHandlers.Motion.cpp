#include "CommandHandlers.h"
#include "CommandHandlersInternal.h"

#include "application/usecases/motion/safety/MotionSafetyUseCase.h"
#include "runtime_execution/application/usecases/motion/homing/HomeAxesUseCase.h"
#include "runtime_execution/application/usecases/motion/manual/ManualMotionControlUseCase.h"
#include "runtime_execution/application/usecases/motion/monitoring/MotionMonitoringUseCase.h"
#include "application/usecases/system/EmergencyStopUseCase.h"

#include <sstream>
#include <vector>

namespace Siligen::Adapters::CLI {

using Siligen::Application::CommandLineConfig;
using Siligen::Application::UseCases::Motion::Homing::HomeAxesRequest;
using Siligen::Application::UseCases::Motion::Homing::HomeAxesUseCase;
using Siligen::Application::UseCases::Motion::Manual::ManualMotionCommand;
using Siligen::Application::UseCases::Motion::Manual::ManualMotionControlUseCase;
using Siligen::Application::UseCases::Motion::Monitoring::MotionMonitoringUseCase;
using Siligen::Application::UseCases::Motion::Safety::MotionSafetyUseCase;
using Siligen::Application::UseCases::System::EmergencyStopRequest;
using Siligen::Application::UseCases::System::EmergencyStopReason;
using Siligen::Application::UseCases::System::EmergencyStopUseCase;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::FromUserInput;
using Siligen::Shared::Types::IsValid;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::ToUserDisplay;

Result<std::vector<LogicalAxisId>> CLICommandHandlers::GetNotHomedAxes(
    const std::vector<LogicalAxisId>& axes) {
    auto home_usecase = container_->Resolve<HomeAxesUseCase>();
    if (!home_usecase) {
        return Result<std::vector<LogicalAxisId>>::Failure(
            Error(ErrorCode::SERVICE_NOT_REGISTERED, "无法解析回零用例", "CLI"));
    }

    std::vector<LogicalAxisId> not_homed;
    for (const auto axis_id : axes) {
        if (!IsValid(axis_id)) {
            return Result<std::vector<LogicalAxisId>>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "轴号无效", "CLI"));
        }

        auto homed_result = home_usecase->IsAxisHomed(axis_id);
        if (homed_result.IsError()) {
            return Result<std::vector<LogicalAxisId>>::Failure(homed_result.GetError());
        }
        if (!homed_result.Value()) {
            not_homed.push_back(axis_id);
        }
    }

    return Result<std::vector<LogicalAxisId>>::Success(not_homed);
}

Result<void> CLICommandHandlers::EnsureAxesHomed(const std::vector<LogicalAxisId>& axes) {
    auto not_homed_result = GetNotHomedAxes(axes);
    if (not_homed_result.IsError()) {
        return Result<void>::Failure(not_homed_result.GetError());
    }

    if (!not_homed_result.Value().empty()) {
        return Result<void>::Failure(BuildAxesNotHomedError(not_homed_result.Value()));
    }

    return Result<void>::Success();
}

void CLICommandHandlers::PrintHomingStatusDiagnostics(const std::vector<LogicalAxisId>& axes) const {
    if (axes.empty()) {
        return;
    }

    auto monitoring_usecase = container_->Resolve<MotionMonitoringUseCase>();
    auto homing_usecase = container_->Resolve<HomeAxesUseCase>();
    if (!monitoring_usecase || !homing_usecase) {
        std::cout << "无法输出回零诊断信息：缺少监控或回零用例" << std::endl;
        return;
    }

    std::cout << "回零状态诊断:" << std::endl;
    for (const auto axis : axes) {
        auto status_result = monitoring_usecase->GetAxisMotionStatus(axis);
        auto homed_result = homing_usecase->IsAxisHomed(axis);

        std::ostringstream line;
        line << "轴 " << ToUserDisplay(axis) << ": ";
        line << "state=";
        if (status_result.IsSuccess()) {
            line << MotionStateToString(status_result.Value().state);
        } else {
            line << "读取失败";
        }
        line << " homed=";
        if (homed_result.IsSuccess()) {
            line << (homed_result.Value() ? "1" : "0");
        } else {
            line << "读取失败";
        }
        std::cout << line.str() << std::endl;
    }
}

int CLICommandHandlers::HandleHome(const CommandLineConfig& config) {
    auto ensure = EnsureConnected(config);
    if (ensure.IsError()) {
        PrintError(ensure.GetError());
        return 1;
    }

    auto usecase = container_->Resolve<HomeAxesUseCase>();
    if (!usecase) {
        std::cout << "无法解析回零用例" << std::endl;
        return 1;
    }

    HomeAxesRequest request;
    request.home_all_axes = config.home_all_axes;
    request.wait_for_completion = config.wait_for_completion;
    request.timeout_ms = config.timeout_ms;
    if (!request.home_all_axes) {
        auto axis_id = FromUserInput(static_cast<int16>(config.axis));
        if (!IsValid(axis_id)) {
            std::cout << "轴号无效" << std::endl;
            return 1;
        }
        request.axes.push_back(axis_id);
    }

    auto result = usecase->Execute(request);
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    const auto& response = result.Value();
    if (!request.wait_for_completion) {
        std::cout << "回零已启动" << std::endl;
        if (!response.status_message.empty()) {
            std::cout << "状态信息: " << response.status_message << std::endl;
        }
        return 0;
    }

    std::cout << "回零完成: 成功轴数=" << response.successfully_homed_axes.size()
              << " 失败轴数=" << response.failed_axes.size() << std::endl;
    if (!response.status_message.empty()) {
        std::cout << "状态信息: " << response.status_message << std::endl;
    }
    if (!response.error_messages.empty()) {
        std::cout << "错误信息:" << std::endl;
        for (const auto& message : response.error_messages) {
            std::cout << "  - " << message << std::endl;
        }
    }

    std::vector<LogicalAxisId> axes_to_diag = response.successfully_homed_axes;
    axes_to_diag.insert(axes_to_diag.end(), response.failed_axes.begin(), response.failed_axes.end());
    PrintHomingStatusDiagnostics(axes_to_diag);
    return response.failed_axes.empty() ? 0 : 1;
}

int CLICommandHandlers::HandleJog(const CommandLineConfig& config) {
    auto ensure = EnsureConnected(config);
    if (ensure.IsError()) {
        PrintError(ensure.GetError());
        return 1;
    }

    double resolved_velocity = config.velocity;
    if (resolved_velocity <= 0.0) {
        double machine_speed = 0.0;
        const auto config_path = ResolveCliConfigPath(config.config_path);
        if (!ReadIniDoubleValue(config_path, "Machine", "max_speed", machine_speed)) {
            std::cout << "未读取到 Machine.max_speed，无法推导默认速度" << std::endl;
            return 1;
        }
        resolved_velocity = machine_speed;
    }

    auto manual_usecase = container_->Resolve<ManualMotionControlUseCase>();
    if (!manual_usecase) {
        std::cout << "无法解析手动运动用例" << std::endl;
        return 1;
    }

    auto axis_id = FromUserInput(static_cast<int16>(config.axis));
    auto result = manual_usecase->StartJogMotionStep(
        axis_id,
        static_cast<int16>(config.direction),
        static_cast<float32>(config.distance),
        static_cast<float32>(resolved_velocity));

    if (result.IsError()) {
        const auto& error = result.GetError();
        switch (error.GetCode()) {
            case ErrorCode::INVALID_AXIS:
                std::cout << "轴号无效" << std::endl;
                break;
            case ErrorCode::INVALID_PARAMETER:
                std::cout << "点动距离必须大于 0" << std::endl;
                break;
            case ErrorCode::VELOCITY_LIMIT_EXCEEDED:
                std::cout << "点动速度必须为正数" << std::endl;
                break;
            default:
                PrintError(error);
                break;
        }
        return 1;
    }

    std::cout << "点动执行成功" << std::endl;
    return 0;
}

int CLICommandHandlers::HandleMove(const CommandLineConfig& config) {
    auto ensure = EnsureConnected(config);
    if (ensure.IsError()) {
        PrintError(ensure.GetError());
        return 1;
    }

    auto axis_id = FromUserInput(static_cast<int16>(config.axis));
    if (!IsValid(axis_id)) {
        std::cout << "轴号无效" << std::endl;
        return 1;
    }

    auto homed = EnsureAxesHomed({axis_id});
    if (homed.IsError()) {
        PrintError(homed.GetError());
        return 1;
    }

    double resolved_velocity = config.velocity;
    double resolved_acceleration = config.acceleration;
    const auto config_path = ResolveCliConfigPath(config.config_path);
    if (resolved_velocity <= 0.0) {
        double machine_speed = 0.0;
        if (!ReadIniDoubleValue(config_path, "Machine", "max_speed", machine_speed)) {
            std::cout << "未读取到 Machine.max_speed，无法推导默认速度" << std::endl;
            return 1;
        }
        resolved_velocity = machine_speed;
    }
    if (resolved_acceleration <= 0.0) {
        double machine_acceleration = 0.0;
        if (!ReadIniDoubleValue(config_path, "Machine", "max_acceleration", machine_acceleration)) {
            std::cout << "未读取到 Machine.max_acceleration，无法推导默认加速度" << std::endl;
            return 1;
        }
        resolved_acceleration = machine_acceleration;
    }
    if (resolved_velocity <= 0.0 || resolved_acceleration <= 0.0) {
        std::cout << "移动速度和加速度必须为正数" << std::endl;
        return 1;
    }

    auto manual_usecase = container_->Resolve<ManualMotionControlUseCase>();
    if (!manual_usecase) {
        std::cout << "无法解析手动运动用例" << std::endl;
        return 1;
    }

    ManualMotionCommand command;
    command.axis = axis_id;
    command.position = static_cast<float32>(config.position);
    command.velocity = static_cast<float32>(resolved_velocity);
    command.acceleration = static_cast<float32>(resolved_acceleration);
    command.relative_move = config.relative_move;

    auto result = manual_usecase->ExecutePointToPointMotion(command);
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    std::cout << "点位移动命令已发送" << std::endl;
    return 0;
}

int CLICommandHandlers::HandleStopAll(const CommandLineConfig& config) {
    auto ensure = EnsureConnected(config);
    if (ensure.IsError()) {
        PrintError(ensure.GetError());
        return 1;
    }

    auto safety_usecase = container_->Resolve<MotionSafetyUseCase>();
    if (!safety_usecase) {
        std::cout << "无法解析运动安全用例" << std::endl;
        return 1;
    }

    auto result = safety_usecase->StopAllAxes(config.immediate_stop);
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    std::cout << "已发送停止所有轴指令" << std::endl;
    return 0;
}

int CLICommandHandlers::HandleEmergencyStop(const CommandLineConfig& config) {
    auto ensure = EnsureConnected(config);
    if (ensure.IsError()) {
        PrintError(ensure.GetError());
        return 1;
    }

    auto usecase = container_->Resolve<EmergencyStopUseCase>();
    if (!usecase) {
        std::cout << "无法解析紧急停止用例" << std::endl;
        return 1;
    }

    EmergencyStopRequest request;
    request.reason = EmergencyStopReason::USER_REQUEST;
    request.detail_message = "CLI emergency stop";

    auto result = usecase->Execute(request);
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    std::cout << "急停已触发" << std::endl;
    return 0;
}

}  // namespace Siligen::Adapters::CLI
