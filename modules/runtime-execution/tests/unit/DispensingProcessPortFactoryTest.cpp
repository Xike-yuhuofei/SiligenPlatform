#include <gtest/gtest.h>

#include "runtime_execution/application/services/dispensing/DispensingProcessPortFactory.h"
#include "shared/types/Error.h"

namespace {

using Siligen::RuntimeExecution::Application::Services::Dispensing::CreateDispensingProcessPort;
using Siligen::Shared::Types::ErrorCode;

TEST(DispensingProcessPortFactoryTest, CreateReturnsRuntimeExecutionProcessPort) {
    const auto process_port = CreateDispensingProcessPort(nullptr, nullptr, nullptr, nullptr, nullptr);

    ASSERT_NE(process_port, nullptr);
    const auto result = process_port->ValidateHardwareConnection();
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::PORT_NOT_INITIALIZED);
}

}  // namespace
