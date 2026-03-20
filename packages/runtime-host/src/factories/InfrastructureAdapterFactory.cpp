#include "InfrastructureAdapterFactory.h"
#include "domain/motion/domain-services/SevenSegmentSCurveProfile.h"

#if SILIGEN_ENABLE_MOCK_MULTICARD
#include "siligen/device/adapters/drivers/multicard/MockMultiCardWrapper.h"
#endif

#if HAS_REAL_MULTICARD
#include "siligen/device/adapters/drivers/multicard/RealMultiCardWrapper.h"
#endif

namespace Siligen {
namespace Infrastructure {
namespace Factories {

std::shared_ptr<Hardware::IMultiCardWrapper> InfrastructureAdapterFactory::CreateMultiCard(
    Shared::Types::HardwareMode mode) {
    if (mode == Shared::Types::HardwareMode::Mock) {
#if SILIGEN_ENABLE_MOCK_MULTICARD
        return std::make_shared<Hardware::MockMultiCardWrapper>();
#elif HAS_REAL_MULTICARD
        return std::make_shared<Hardware::RealMultiCardWrapper>();
#else
        return nullptr;
#endif
    }

#if HAS_REAL_MULTICARD
    return std::make_shared<Hardware::RealMultiCardWrapper>();
#else
#if SILIGEN_ENABLE_MOCK_MULTICARD
    return std::make_shared<Hardware::MockMultiCardWrapper>();
#else
    return nullptr;
#endif
#endif
}

std::shared_ptr<Adapters::HardwareConnectionAdapter> InfrastructureAdapterFactory::CreateHardwareConnectionAdapter(
    std::shared_ptr<Hardware::IMultiCardWrapper> multiCard,
    const Shared::Types::HardwareConfiguration& config,
    Shared::Types::HardwareMode mode) {
    return std::make_shared<Adapters::HardwareConnectionAdapter>(multiCard, mode, config);
}

std::shared_ptr<Adapters::Hardware::HardwareTestAdapter> InfrastructureAdapterFactory::CreateHardwareTestAdapter(
    std::shared_ptr<Hardware::IMultiCardWrapper> multiCard,
    const Shared::Types::HardwareConfiguration& config,
    const std::array<Domain::Configuration::Ports::HomingConfig, 4>& homing_configs) {
    return std::make_shared<Adapters::Hardware::HardwareTestAdapter>(multiCard, config, homing_configs);
}

std::shared_ptr<Domain::Motion::Ports::IVelocityProfilePort>
InfrastructureAdapterFactory::CreateSevenSegmentSCurveAdapter() {
    return std::make_shared<Domain::Motion::DomainServices::SevenSegmentSCurveProfile>();
}

}  // namespace Factories
}  // namespace Infrastructure
}  // namespace Siligen
