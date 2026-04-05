#include "application/services/dispensing/WorkflowDispensingProcessOperationsProvider.h"

#include <gtest/gtest.h>

namespace {

using WorkflowDispensingProcessOperationsProvider =
    Siligen::Application::Services::Dispensing::WorkflowDispensingProcessOperationsProvider;

TEST(WorkflowDispensingProcessOperationsProviderTest, CreatesOperationsObjectForRuntimeHostConsumption) {
    WorkflowDispensingProcessOperationsProvider provider;

    const auto operations = provider.CreateOperations(nullptr, nullptr, nullptr, nullptr, nullptr);

    ASSERT_NE(operations, nullptr);
    const auto result = operations->ValidateHardwareConnection();
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), Siligen::Shared::Types::ErrorCode::PORT_NOT_INITIALIZED);
}

}  // namespace
