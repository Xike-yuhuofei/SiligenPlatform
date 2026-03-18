#pragma once

#include "shared/types/HardwareConfiguration.h"
#include "domain/configuration/ports/IConfigurationPort.h"
#include "domain/motion/ports/IVelocityProfilePort.h"
#include "modules/device-hal/src/adapters/motion/controller/connection/HardwareConnectionAdapter.h"
#include "modules/device-hal/src/adapters/diagnostics/health/testing/HardwareTestAdapter.h"
#include "modules/device-hal/src/adapters/motion/controller/control/MultiCardMotionAdapter.h"
#include "modules/device-hal/src/drivers/multicard/IMultiCardWrapper.h"

#include <memory>
#include <array>

namespace Siligen {
namespace Infrastructure {
namespace Factories {

using HardwareTestAdapter = Adapters::Hardware::HardwareTestAdapter;

/**
 * @brief 基础设施层工厂
 *
 * 负责创建所有infrastructure层的对象实例
 */
class InfrastructureAdapterFactory {
   public:
    InfrastructureAdapterFactory() = default;
    ~InfrastructureAdapterFactory() = default;

    // ========== 硬件相关 ==========
    std::shared_ptr<Hardware::IMultiCardWrapper> CreateMultiCard(
        Shared::Types::HardwareMode mode = Shared::Types::HardwareMode::Real);

    std::shared_ptr<Adapters::HardwareConnectionAdapter> CreateHardwareConnectionAdapter(
        std::shared_ptr<Hardware::IMultiCardWrapper> multiCard,
        const Shared::Types::HardwareConfiguration& config,
        Shared::Types::HardwareMode mode = Shared::Types::HardwareMode::Real);

    std::shared_ptr<HardwareTestAdapter> CreateHardwareTestAdapter(
        std::shared_ptr<Hardware::IMultiCardWrapper> multiCard,
        const Shared::Types::HardwareConfiguration& config,
        const std::array<Domain::Configuration::Ports::HomingConfig, 4>& homing_configs);

    // ========== 速度规划相关 ==========
    std::shared_ptr<Domain::Motion::Ports::IVelocityProfilePort> CreateSevenSegmentSCurveAdapter();
};

}  // namespace Factories
}  // namespace Infrastructure
}  // namespace Siligen
