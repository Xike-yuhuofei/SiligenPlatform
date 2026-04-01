#include "ApplicationContainer.h"

#include "application/usecases/motion/coordination/MotionCoordinationUseCase.h"
#include "application/usecases/motion/homing/EnsureAxesReadyZeroUseCase.h"
#include "application/usecases/motion/homing/HomeAxesUseCase.h"
#include "application/usecases/motion/initialization/MotionInitializationUseCase.h"
#include "application/usecases/motion/interpolation/InterpolationPlanningUseCase.h"
#include "application/usecases/motion/manual/ManualMotionControlUseCase.h"
#include "application/usecases/motion/monitoring/MotionMonitoringUseCase.h"
#include "runtime_execution/application/usecases/motion/MotionControlUseCase.h"
#include "application/usecases/motion/safety/MotionSafetyUseCase.h"
#include "application/usecases/motion/trajectory/ExecuteTrajectoryUseCase.h"
#include "domain/motion/domain-services/JogController.h"
#include "domain/motion/domain-services/ReadyZeroDecisionService.h"
#include "domain/motion/domain-services/VelocityProfileService.h"
#include "services/motion/HardLimitMonitorService.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/types/Error.h"

#include <memory>
#include <string>
#include <stdexcept>

#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "ApplicationContainer.Motion"

namespace Siligen::Application::Container {
namespace {

using Siligen::Application::UseCases::Motion::IMotionHomingOperations;
using Siligen::Application::UseCases::Motion::IMotionManualOperations;
using Siligen::Application::UseCases::Motion::IMotionMonitoringOperations;
using Siligen::Application::UseCases::Motion::Homing::EnsureAxesReadyZeroRequest;
using Siligen::Application::UseCases::Motion::Homing::EnsureAxesReadyZeroResponse;
using Siligen::Application::UseCases::Motion::Homing::EnsureAxesReadyZeroUseCase;
using Siligen::Application::UseCases::Motion::Homing::HomeAxesRequest;
using Siligen::Application::UseCases::Motion::Homing::HomeAxesResponse;
using Siligen::Application::UseCases::Motion::Homing::HomeAxesUseCase;
using Siligen::Application::UseCases::Motion::Manual::ManualMotionCommand;
using Siligen::Application::UseCases::Motion::Manual::ManualMotionControlUseCase;
using Siligen::Application::UseCases::Motion::Monitoring::MotionMonitoringUseCase;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::int16;
using Siligen::Shared::Types::uint32;

constexpr const char* kMotionControlBridgeSource = "ApplicationContainer.Motion";

template <typename TResult>
Result<TResult> MissingBridgeDependency(const char* dependency_name) {
    return Result<TResult>::Failure(Error(
        ErrorCode::PORT_NOT_INITIALIZED,
        std::string(dependency_name) + " not available",
        kMotionControlBridgeSource));
}

Result<void> MissingBridgeVoidDependency(const char* dependency_name) {
    return Result<void>::Failure(Error(
        ErrorCode::PORT_NOT_INITIALIZED,
        std::string(dependency_name) + " not available",
        kMotionControlBridgeSource));
}

class WorkflowMotionHomingOperations final : public IMotionHomingOperations {
   public:
    WorkflowMotionHomingOperations(
        std::shared_ptr<HomeAxesUseCase> home_use_case,
        std::shared_ptr<EnsureAxesReadyZeroUseCase> ensure_ready_zero_use_case)
        : home_use_case_(std::move(home_use_case)),
          ensure_ready_zero_use_case_(std::move(ensure_ready_zero_use_case)) {}

    Result<HomeAxesResponse> Home(const HomeAxesRequest& request) override {
        if (!home_use_case_) {
            return MissingBridgeDependency<HomeAxesResponse>("HomeAxesUseCase");
        }
        return home_use_case_->Execute(request);
    }

    Result<EnsureAxesReadyZeroResponse> EnsureAxesReadyZero(const EnsureAxesReadyZeroRequest& request) override {
        if (!ensure_ready_zero_use_case_) {
            return MissingBridgeDependency<EnsureAxesReadyZeroResponse>("EnsureAxesReadyZeroUseCase");
        }
        return ensure_ready_zero_use_case_->Execute(request);
    }

    Result<bool> IsAxisHomed(Siligen::Shared::Types::LogicalAxisId axis) const override {
        if (!home_use_case_) {
            return MissingBridgeDependency<bool>("HomeAxesUseCase");
        }
        return home_use_case_->IsAxisHomed(axis);
    }

   private:
    std::shared_ptr<HomeAxesUseCase> home_use_case_;
    std::shared_ptr<EnsureAxesReadyZeroUseCase> ensure_ready_zero_use_case_;
};

class WorkflowMotionManualOperations final : public IMotionManualOperations {
   public:
    explicit WorkflowMotionManualOperations(std::shared_ptr<ManualMotionControlUseCase> manual_use_case)
        : manual_use_case_(std::move(manual_use_case)) {}

    Result<void> ExecutePointToPointMotion(const ManualMotionCommand& command, bool invalidate_homing) override {
        if (!manual_use_case_) {
            return MissingBridgeVoidDependency("ManualMotionControlUseCase");
        }
        return manual_use_case_->ExecutePointToPointMotion(command, invalidate_homing);
    }

    Result<void> StartJog(
        Siligen::Shared::Types::LogicalAxisId axis,
        int16 direction,
        Siligen::Shared::Types::float32 velocity) override {
        if (!manual_use_case_) {
            return MissingBridgeVoidDependency("ManualMotionControlUseCase");
        }
        return manual_use_case_->StartJogMotion(axis, direction, velocity);
    }

    Result<void> StopJog(Siligen::Shared::Types::LogicalAxisId axis) override {
        if (!manual_use_case_) {
            return MissingBridgeVoidDependency("ManualMotionControlUseCase");
        }
        return manual_use_case_->StopJogMotion(axis);
    }

   private:
    std::shared_ptr<ManualMotionControlUseCase> manual_use_case_;
};

class WorkflowMotionMonitoringOperations final : public IMotionMonitoringOperations {
   public:
    explicit WorkflowMotionMonitoringOperations(std::shared_ptr<MotionMonitoringUseCase> monitoring_use_case)
        : monitoring_use_case_(std::move(monitoring_use_case)) {}

    Result<Domain::Motion::Ports::MotionStatus> GetAxisMotionStatus(
        Siligen::Shared::Types::LogicalAxisId axis) const override {
        if (!monitoring_use_case_) {
            return MissingBridgeDependency<Domain::Motion::Ports::MotionStatus>("MotionMonitoringUseCase");
        }
        return monitoring_use_case_->GetAxisMotionStatus(axis);
    }

    Result<std::vector<Domain::Motion::Ports::MotionStatus>> GetAllAxesMotionStatus() const override {
        if (!monitoring_use_case_) {
            return MissingBridgeDependency<std::vector<Domain::Motion::Ports::MotionStatus>>(
                "MotionMonitoringUseCase");
        }
        return monitoring_use_case_->GetAllAxesMotionStatus();
    }

    Result<Point2D> GetCurrentPosition() const override {
        if (!monitoring_use_case_) {
            return MissingBridgeDependency<Point2D>("MotionMonitoringUseCase");
        }
        return monitoring_use_case_->GetCurrentPosition();
    }

    Result<Domain::Motion::Ports::CoordinateSystemStatus> GetCoordinateSystemStatus(int16 coord_sys)
        const override {
        if (!monitoring_use_case_) {
            return MissingBridgeDependency<Domain::Motion::Ports::CoordinateSystemStatus>(
                "MotionMonitoringUseCase");
        }
        return monitoring_use_case_->GetCoordinateSystemStatus(coord_sys);
    }

    Result<uint32> GetInterpolationBufferSpace(int16 coord_sys) const override {
        if (!monitoring_use_case_) {
            return MissingBridgeDependency<uint32>("MotionMonitoringUseCase");
        }
        return monitoring_use_case_->GetInterpolationBufferSpace(coord_sys);
    }

    Result<uint32> GetLookAheadBufferSpace(int16 coord_sys) const override {
        if (!monitoring_use_case_) {
            return MissingBridgeDependency<uint32>("MotionMonitoringUseCase");
        }
        return monitoring_use_case_->GetLookAheadBufferSpace(coord_sys);
    }

    Result<bool> ReadLimitStatus(Siligen::Shared::Types::LogicalAxisId axis, bool positive) const override {
        if (!monitoring_use_case_) {
            return MissingBridgeDependency<bool>("MotionMonitoringUseCase");
        }
        return monitoring_use_case_->ReadLimitStatus(axis, positive);
    }

    Result<bool> ReadServoAlarmStatus(Siligen::Shared::Types::LogicalAxisId axis) const override {
        if (!monitoring_use_case_) {
            return MissingBridgeDependency<bool>("MotionMonitoringUseCase");
        }
        return monitoring_use_case_->ReadServoAlarmStatus(axis);
    }

   private:
    std::shared_ptr<MotionMonitoringUseCase> monitoring_use_case_;
};

}  // namespace

void ApplicationContainer::ValidateMotionPorts() {
    if (!motion_runtime_port_) {
        throw std::runtime_error("IMotionRuntimePort 未注册");
    }
    if (!interpolation_port_) {
        throw std::runtime_error("IInterpolationPort 未注册");
    }
    if (!velocity_profile_port_) {
        SILIGEN_LOG_WARNING("IVelocityProfilePort 未注册");
    }
}

void ApplicationContainer::ConfigureMotionServices() {
    auto motion_jog_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IJogControlPort>(motion_runtime_port_)
        : motion_jog_port_;
    auto motion_state_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IMotionStatePort>(motion_runtime_port_)
        : motion_state_port_;
    auto io_control_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IIOControlPort>(motion_runtime_port_)
        : io_control_port_;
    auto position_control_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IPositionControlPort>(motion_runtime_port_)
        : position_control_port_;
    jog_controller_ = std::make_shared<Domain::Motion::DomainServices::JogController>(
        motion_jog_port,
        motion_state_port);
    RegisterService<Domain::Motion::DomainServices::JogController>(jog_controller_);
    SILIGEN_LOG_INFO("JogController registered");

    if (velocity_profile_port_) {
        velocity_profile_service_ =
            std::make_shared<Domain::Motion::DomainServices::VelocityProfileService>(velocity_profile_port_);
        RegisterService<Domain::Motion::DomainServices::VelocityProfileService>(velocity_profile_service_);
        SILIGEN_LOG_INFO("VelocityProfileService registered");
    } else {
        SILIGEN_LOG_WARNING("VelocityProfileService not registered: IVelocityProfilePort missing");
    }

    if (io_control_port && position_control_port) {
        hard_limit_monitor_ =
            std::make_shared<Siligen::Application::Services::Motion::HardLimitMonitorService>(
                io_control_port,
                position_control_port);
        SILIGEN_LOG_INFO("HardLimitMonitorService registered");
    } else {
        SILIGEN_LOG_WARNING("HardLimitMonitorService not registered: IO or position control port missing");
    }
}

template<>
std::shared_ptr<UseCases::Motion::Homing::HomeAxesUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::Homing::HomeAxesUseCase>() {
    auto motion_connection_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IMotionConnectionPort>(motion_runtime_port_)
        : motion_connection_port_;
    auto homing_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IHomingPort>(motion_runtime_port_)
        : homing_port_;
    auto motion_state_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IMotionStatePort>(motion_runtime_port_)
        : motion_state_port_;
    return std::make_shared<UseCases::Motion::Homing::HomeAxesUseCase>(
        homing_port,
        config_port_,
        motion_connection_port,
        event_port_,
        motion_state_port);
}

template<>
std::shared_ptr<UseCases::Motion::Homing::EnsureAxesReadyZeroUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::Homing::EnsureAxesReadyZeroUseCase>() {
    return std::make_shared<UseCases::Motion::Homing::EnsureAxesReadyZeroUseCase>(
        Resolve<UseCases::Motion::Homing::HomeAxesUseCase>(),
        Resolve<UseCases::Motion::Manual::ManualMotionControlUseCase>(),
        Resolve<UseCases::Motion::Monitoring::MotionMonitoringUseCase>(),
        config_port_,
        std::make_shared<Domain::Motion::DomainServices::ReadyZeroDecisionService>());
}

template<>
std::shared_ptr<UseCases::Motion::MotionControlUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::MotionControlUseCase>() {
    auto home_use_case = Resolve<UseCases::Motion::Homing::HomeAxesUseCase>();
    auto ensure_ready_zero_use_case = Resolve<UseCases::Motion::Homing::EnsureAxesReadyZeroUseCase>();
    auto manual_use_case = Resolve<UseCases::Motion::Manual::ManualMotionControlUseCase>();
    auto monitoring_use_case = Resolve<UseCases::Motion::Monitoring::MotionMonitoringUseCase>();

    return std::make_shared<UseCases::Motion::MotionControlUseCase>(
        std::make_shared<WorkflowMotionHomingOperations>(home_use_case, ensure_ready_zero_use_case),
        std::make_shared<WorkflowMotionManualOperations>(manual_use_case),
        std::make_shared<WorkflowMotionMonitoringOperations>(monitoring_use_case));
}

template<>
std::shared_ptr<UseCases::Motion::Trajectory::ExecuteTrajectoryUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::Trajectory::ExecuteTrajectoryUseCase>() {
    auto position_control_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IPositionControlPort>(motion_runtime_port_)
        : position_control_port_;
    return std::make_shared<UseCases::Motion::Trajectory::ExecuteTrajectoryUseCase>(
        position_control_port,
        trigger_port_,
        event_port_);
}

template<>
std::shared_ptr<UseCases::Motion::Manual::ManualMotionControlUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::Manual::ManualMotionControlUseCase>() {
    auto position_control_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IPositionControlPort>(motion_runtime_port_)
        : position_control_port_;
    auto homing_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IHomingPort>(motion_runtime_port_)
        : homing_port_;
    return std::make_shared<UseCases::Motion::Manual::ManualMotionControlUseCase>(
        position_control_port,
        jog_controller_,
        homing_port);
}

template<>
std::shared_ptr<UseCases::Motion::Initialization::MotionInitializationUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::Initialization::MotionInitializationUseCase>() {
    auto motion_connection_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IMotionConnectionPort>(motion_runtime_port_)
        : motion_connection_port_;
    auto axis_control_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IAxisControlPort>(motion_runtime_port_)
        : axis_control_port_;
    auto io_control_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IIOControlPort>(motion_runtime_port_)
        : io_control_port_;
    return std::make_shared<UseCases::Motion::Initialization::MotionInitializationUseCase>(
        motion_connection_port,
        axis_control_port,
        io_control_port);
}

template<>
std::shared_ptr<UseCases::Motion::Safety::MotionSafetyUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::Safety::MotionSafetyUseCase>() {
    auto position_control_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IPositionControlPort>(motion_runtime_port_)
        : position_control_port_;
    return std::make_shared<UseCases::Motion::Safety::MotionSafetyUseCase>(position_control_port);
}

template<>
std::shared_ptr<UseCases::Motion::Monitoring::MotionMonitoringUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::Monitoring::MotionMonitoringUseCase>() {
    auto motion_state_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IMotionStatePort>(motion_runtime_port_)
        : motion_state_port_;
    auto io_control_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IIOControlPort>(motion_runtime_port_)
        : io_control_port_;
    auto homing_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IHomingPort>(motion_runtime_port_)
        : homing_port_;
    return std::make_shared<UseCases::Motion::Monitoring::MotionMonitoringUseCase>(
        motion_state_port,
        io_control_port,
        homing_port,
        interpolation_port_);
}

template<>
std::shared_ptr<UseCases::Motion::Coordination::MotionCoordinationUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::Coordination::MotionCoordinationUseCase>() {
    auto io_control_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IIOControlPort>(motion_runtime_port_)
        : io_control_port_;
    auto axis_control_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IAxisControlPort>(motion_runtime_port_)
        : axis_control_port_;
    return std::make_shared<UseCases::Motion::Coordination::MotionCoordinationUseCase>(
        interpolation_port_,
        io_control_port,
        axis_control_port,
        trigger_port_);
}

template<>
std::shared_ptr<UseCases::Motion::Interpolation::InterpolationPlanningUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::Interpolation::InterpolationPlanningUseCase>() {
    return std::make_shared<UseCases::Motion::Interpolation::InterpolationPlanningUseCase>();
}

}  // namespace Siligen::Application::Container

