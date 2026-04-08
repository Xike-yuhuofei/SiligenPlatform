#include "dxf_geometry/adapters/planning/dxf/DXFAdapterFactory.h"

#include <gtest/gtest.h>

namespace {

using Siligen::Infrastructure::Adapters::Parsing::DXFAdapterFactory;
using Siligen::Shared::Types::ErrorCode;

}  // namespace

TEST(DXFAdapterFactoryTest, MockAdapterProvidesDeterministicDXFResponses) {
    auto adapter_result = DXFAdapterFactory::CreateDXFPathSourceAdapter(DXFAdapterFactory::AdapterType::MOCK);

    ASSERT_TRUE(adapter_result.IsSuccess()) << adapter_result.GetError().ToString();
    auto adapter = adapter_result.Value();
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
    auto adapter_result = DXFAdapterFactory::CreateDXFPathSourceAdapter(DXFAdapterFactory::AdapterType::REMOTE);

    ASSERT_TRUE(adapter_result.IsError());
    EXPECT_EQ(DXFAdapterFactory::GetCurrentAdapterType(), DXFAdapterFactory::AdapterType::REMOTE);
    EXPECT_EQ(adapter_result.GetError().GetCode(), ErrorCode::NOT_IMPLEMENTED);
}

TEST(DXFAdapterFactoryTest, LocalAdapterRejectsDirectDxfInputUntilPbPrepared) {
    auto adapter_result = DXFAdapterFactory::CreateDXFPathSourceAdapter(DXFAdapterFactory::AdapterType::LOCAL);

    ASSERT_TRUE(adapter_result.IsSuccess()) << adapter_result.GetError().ToString();
    auto adapter = adapter_result.Value();
    ASSERT_NE(adapter, nullptr);

    auto load_result = adapter->LoadDXFFile("direct_input.dxf");
    ASSERT_TRUE(load_result.IsError());
    EXPECT_EQ(load_result.GetError().GetCode(), ErrorCode::FILE_FORMAT_INVALID);
}
