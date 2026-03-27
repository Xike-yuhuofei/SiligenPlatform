#include "domain/motion/ports/IIOControlPort.h"
#include "domain/motion/ports/IMotionRuntimePort.h"
#include "runtime_execution/contracts/motion/IIOControlPort.h"
#include "runtime_execution/contracts/motion/IMotionRuntimePort.h"

#include <gtest/gtest.h>

#include <type_traits>

namespace {

TEST(NoRuntimeControlLeakTest, DomainIoPortHeaderIsNowCompatibilityShimToM9OwnerContract) {
    EXPECT_TRUE((std::is_same_v<
        Siligen::Domain::Motion::Ports::IIOControlPort,
        Siligen::RuntimeExecution::Contracts::Motion::IIOControlPort>));
    EXPECT_TRUE((std::is_same_v<
        Siligen::Domain::Motion::Ports::IOStatus,
        Siligen::RuntimeExecution::Contracts::Motion::IOStatus>));
}

TEST(NoRuntimeControlLeakTest, DomainMotionRuntimeHeaderIsNowCompatibilityShimToM9OwnerContract) {
    EXPECT_TRUE((std::is_same_v<
        Siligen::Domain::Motion::Ports::IMotionRuntimePort,
        Siligen::RuntimeExecution::Contracts::Motion::IMotionRuntimePort>));
    EXPECT_TRUE((std::is_abstract_v<Siligen::RuntimeExecution::Contracts::Motion::IMotionRuntimePort>));
}

}  // namespace
