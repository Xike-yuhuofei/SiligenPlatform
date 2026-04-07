#pragma once

#include "shared/types/Result.h"

namespace Siligen::Application::UseCases::System {

class IHardLimitMonitor {
   public:
    virtual ~IHardLimitMonitor() = default;

    virtual Shared::Types::Result<void> Start() noexcept = 0;
    virtual Shared::Types::Result<void> Stop() noexcept = 0;
    virtual bool IsRunning() const noexcept = 0;
};

}  // namespace Siligen::Application::UseCases::System
