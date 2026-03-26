#pragma once

#include <cstdint>

namespace Siligen::Domain::Dispensing::Ports {

class IDispensingExecutionObserver {
   public:
    virtual ~IDispensingExecutionObserver() = default;
    virtual void OnMotionStart() noexcept = 0;
    virtual void OnMotionStop() noexcept = 0;
    virtual void OnProgress(std::uint32_t /*executed_segments*/, std::uint32_t /*total_segments*/) noexcept {}
    virtual void OnPauseStateChanged(bool /*paused*/) noexcept {}
};

}  // namespace Siligen::Domain::Dispensing::Ports
