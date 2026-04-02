#include "support/DxfPbPreparationServiceTestSupport.h"

#include <gtest/gtest.h>

#include <memory>
#include <type_traits>
#include <utility>

namespace {

using Siligen::DxfGeometry::Tests::Support::DxfPbPreparationService;
using Siligen::DxfGeometry::Tests::Support::FakeConfigurationPort;
using Siligen::DxfGeometry::Tests::Support::ReadTextFile;
using Siligen::DxfGeometry::Tests::Support::ScopedTempDir;
using Siligen::DxfGeometry::Tests::Support::WriteTextFile;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

static_assert(std::is_default_constructible_v<DxfPbPreparationService>);
static_assert(
    std::is_constructible_v<DxfPbPreparationService, std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort>>);
static_assert(std::is_same_v<decltype(std::declval<const DxfPbPreparationService&>().EnsurePbReady(
                                 std::declval<const std::string&>())),
                             Result<std::string>>);

TEST(DxfPbPreparationServiceContractsTest, ReturnsExistingPbPathWhenPbAlreadyValid) {
    ScopedTempDir workspace("dxf_pb_contract_passthrough");
    const auto pb_path = workspace.Path() / "sample.pb";
    WriteTextFile(pb_path, "cached-pb");

    DxfPbPreparationService service;
    auto result = service.EnsurePbReady(pb_path.string());
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();
    EXPECT_EQ(result.Value(), pb_path.string());
    EXPECT_EQ(ReadTextFile(pb_path), "cached-pb");
}

TEST(DxfPbPreparationServiceContractsTest, RejectsUnsupportedInputExtension) {
    ScopedTempDir workspace("dxf_pb_contract_invalid_extension");
    const auto txt_path = workspace.Path() / "sample.txt";
    WriteTextFile(txt_path, "not dxf");

    DxfPbPreparationService service(std::make_shared<FakeConfigurationPort>());
    auto result = service.EnsurePbReady(txt_path.string());
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::FILE_FORMAT_INVALID);
}

}  // namespace
