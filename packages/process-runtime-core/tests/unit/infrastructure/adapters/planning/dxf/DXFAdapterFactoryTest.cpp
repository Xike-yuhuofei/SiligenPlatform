#include "infrastructure/adapters/planning/dxf/DXFAdapterFactory.h"

#include <gtest/gtest.h>

namespace {

using Siligen::Infrastructure::Adapters::Parsing::DXFAdapterFactory;

}  // namespace

TEST(DXFAdapterFactoryTest, MockAdapterProvidesDeterministicDXFResponses) {
    auto adapter = DXFAdapterFactory::CreateDXFPathSourceAdapter(DXFAdapterFactory::AdapterType::MOCK);

    ASSERT_NE(adapter, nullptr);
    EXPECT_EQ(DXFAdapterFactory::GetCurrentAdapterType(), DXFAdapterFactory::AdapterType::MOCK);

    auto validation = adapter->ValidateDXFFile("mock_input.dxf");
    ASSERT_TRUE(validation.IsSuccess()) << validation.GetError().ToString();
    EXPECT_TRUE(validation.Value().is_valid);
    EXPECT_EQ(validation.Value().format_version, "MOCK");

    auto load_result = adapter->LoadDXFFile("mock_input.dxf");
    ASSERT_TRUE(load_result.IsSuccess()) << load_result.GetError().ToString();
    EXPECT_TRUE(load_result.Value().success);
    EXPECT_EQ(load_result.Value().source_format, "MOCK_DXF");

    auto preprocess = adapter->RequiresPreprocessing("mock_input.dxf");
    ASSERT_TRUE(preprocess.IsSuccess()) << preprocess.GetError().ToString();
    EXPECT_FALSE(preprocess.Value());
}

TEST(DXFAdapterFactoryTest, HealthCheckRejectsNullAdapter) {
    EXPECT_FALSE(DXFAdapterFactory::CheckAdapterHealth(nullptr));
}

TEST(DXFAdapterFactoryTest, RemoteAdapterStubFailsExplicitlyInsteadOfFallingBackToLocal) {
    auto adapter = DXFAdapterFactory::CreateDXFPathSourceAdapter(DXFAdapterFactory::AdapterType::REMOTE);

    ASSERT_NE(adapter, nullptr);
    EXPECT_EQ(DXFAdapterFactory::GetCurrentAdapterType(), DXFAdapterFactory::AdapterType::REMOTE);
    EXPECT_FALSE(DXFAdapterFactory::CheckAdapterHealth(adapter));

    auto validation = adapter->ValidateDXFFile("remote_input.dxf");
    ASSERT_TRUE(validation.IsError());
    EXPECT_EQ(validation.GetError().GetCode(), Siligen::Shared::Types::ErrorCode::NOT_IMPLEMENTED);

    auto load_result = adapter->LoadDXFFile("remote_input.dxf");
    ASSERT_TRUE(load_result.IsError());
    EXPECT_EQ(load_result.GetError().GetCode(), Siligen::Shared::Types::ErrorCode::NOT_IMPLEMENTED);
}
