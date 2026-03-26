#include "MotionBufferController.h"

#include "shared/types/Error.h"
#include "shared/types/Result.h"

#include <chrono>
#include <thread>

namespace Siligen::Domain::Motion {

Shared::Types::Result<std::shared_ptr<MotionBufferController>> MotionBufferController::Create(
    std::shared_ptr<Ports::IInterpolationPort> interpolation_port) {
    if (!interpolation_port) {
        return Shared::Types::Result<std::shared_ptr<MotionBufferController>>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::INVALID_PARAMETER,
                                 "Interpolation port cannot be null",
                                 "MotionBufferController"));
    }

    return Shared::Types::Result<std::shared_ptr<MotionBufferController>>::Success(
        std::shared_ptr<MotionBufferController>(new MotionBufferController(std::move(interpolation_port))));
}

MotionBufferController::MotionBufferController(std::shared_ptr<Ports::IInterpolationPort> interpolation_port)
    : interpolation_port_(std::move(interpolation_port)) {}

int32 MotionBufferController::GetBufferSpace(short crd_num) {
    if (!interpolation_port_) return 0;

    auto result = interpolation_port_->GetInterpolationBufferSpace(static_cast<int16>(crd_num));
    if (!result.IsSuccess()) return 0;

    int32 space = static_cast<int32>(result.Value());
    return space < 0 ? 0 : space;
}

bool MotionBufferController::WaitForBufferReady(short crd_num, int32 required_space, int32 timeout_ms) {
    auto start = std::chrono::steady_clock::now();
    while (true) {
        if (GetBufferSpace(crd_num) >= required_space) return true;
        auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        if (elapsed >= timeout_ms) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

bool MotionBufferController::ClearBuffer(short crd_num) {
    if (!interpolation_port_) return false;

    auto result = interpolation_port_->ClearInterpolationBuffer(static_cast<int16>(crd_num));
    return result.IsSuccess();
}

bool MotionBufferController::CheckBufferHealth(short crd_num, uint32& underflow_count) {
    int32 space = GetBufferSpace(crd_num);
    if (space < 10) {
        underflow_count++;
        return HandleBufferUnderflow(crd_num);
    }
    return space >= 0;
}

bool MotionBufferController::HandleBufferOverflow(short crd_num) {
    return WaitForBufferReady(crd_num, 10, 5000);
}

bool MotionBufferController::HandleBufferUnderflow(short) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return true;
}

}  // namespace Siligen::Domain::Motion

