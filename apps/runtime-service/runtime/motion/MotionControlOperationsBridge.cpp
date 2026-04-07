#include "runtime/motion/MotionControlOperationsBridge.h"

#include "application/usecases/motion/homing/EnsureAxesReadyZeroUseCase.h"
#include "application/usecases/motion/homing/HomeAxesUseCase.h"
#include "application/usecases/motion/manual/ManualMotionControlUseCase.h"
#include "application/usecases/motion/monitoring/MotionMonitoringUseCase.h"
#include "shared/types/Error.h"

#include <string>
#include <utility>

namespace Siligen::Runtime::Service::Motion {
namespace {

using Siligen::Application::UseCases::Motion::Homing::EnsureAxesReadyZeroRequest;
using Siligen::Application::UseCases::Motion::Homing::EnsureAxesReadyZeroResponse;
using Siligen::Application::UseCases::Motion::Homing::EnsureAxesReadyZeroUseCase;
using Siligen::Application::UseCases::Motion::Homing::HomeAxesRequest;
using Siligen::Application::UseCases::Motion::Homing::HomeAxesResponse;
using Siligen::Application::UseCases::Motion::Homing::HomeAxesUseCase;
using Siligen::Application::UseCases::Motion::IMotionHomingOperations;
using Siligen::Application::UseCases::Motion::IMotionManualOperations;
using Siligen::Application::UseCases::Motion::IMotionMonitoringOperations;
using Siligen::Application::UseCases::Motion::Manual::ManualMotionCommand;
using Siligen::Application::UseCases::Motion::Manual::ManualMotionControlUseCase;
using Siligen::Application::UseCases::Motion::Monitoring::MotionMonitoringUseCase;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::int16;
using Siligen::Shared::Types::uint32;

constexpr const char* kErrorSource = "MotionControlOperationsBridge";

template <typename TResult>
Result<TResult> MissingBridgeDependency(const char* dependency_name) {
    return Result<TResult>::Failure(Error(
        ErrorCode::PORT_NOT_INITIALIZED,
        std::string(dependency_name) + " not available",
        kErrorSource));
}

Result<void> MissingBridgeVoidDependency(const char* dependency_name) {
    return Result<void>::Failure(Error(
        ErrorCode::PORT_NOT_INITIALIZED,
        std::string(dependency_name) + " not available",
        kErrorSource));
}

class MotionHomingOperations final : public IMotionHomingOperations {
   public:
    MotionHomingOperations(
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

class MotionManualOperations final : public IMotionManualOperations {
   public:
    explicit MotionManualOperations(std::shared_ptr<ManualMotionControlUseCase> manual_use_case)
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

class MotionMonitoringOperations final : public IMotionMonitoringOperations {
   public:
    explicit MotionMonitoringOperations(std::shared_ptr<MotionMonitoringUseCase> monitoring_use_case)
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

MotionOperationsBundle CreateMotionOperations(
    std::shared_ptr<HomeAxesUseCase> home_use_case,
    std::shared_ptr<EnsureAxesReadyZeroUseCase> ensure_ready_zero_use_case,
    std::shared_ptr<ManualMotionControlUseCase> manual_use_case,
    std::shared_ptr<MotionMonitoringUseCase> monitoring_use_case) {
    MotionOperationsBundle bundle;
    bundle.homing_operations =
        std::make_shared<MotionHomingOperations>(std::move(home_use_case), std::move(ensure_ready_zero_use_case));
    bundle.manual_operations = std::make_shared<MotionManualOperations>(std::move(manual_use_case));
    bundle.monitoring_operations =
        std::make_shared<MotionMonitoringOperations>(std::move(monitoring_use_case));
    return bundle;
}

}  // namespace Siligen::Runtime::Service::Motion
