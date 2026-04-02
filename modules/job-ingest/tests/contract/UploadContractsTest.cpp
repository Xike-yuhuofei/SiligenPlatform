#include "job_ingest/contracts/dispensing/UploadContracts.h"
#include "job_ingest/contracts/storage/IFileStoragePort.h"
#include "shared/types/Result.h"

#include <gtest/gtest.h>

#include <type_traits>
#include <utility>
#include <vector>

namespace {

using Siligen::Application::UseCases::Dispensing::IUploadFilePort;
using Siligen::Application::UseCases::Dispensing::UploadRequest;
using Siligen::Application::UseCases::Dispensing::UploadResponse;
using Siligen::JobIngest::Contracts::Storage::FileData;
using Siligen::JobIngest::Contracts::Storage::IFileStoragePort;
using Siligen::Shared::Types::Result;

static_assert(std::is_default_constructible_v<UploadRequest>);
static_assert(std::is_default_constructible_v<UploadResponse>);
static_assert(std::is_default_constructible_v<FileData>);
static_assert(std::is_abstract_v<IUploadFilePort>);
static_assert(std::is_abstract_v<IFileStoragePort>);
static_assert(std::is_same_v<decltype(std::declval<IUploadFilePort&>().Execute(std::declval<const UploadRequest&>())),
                             Result<UploadResponse>>);
static_assert(std::is_same_v<decltype(std::declval<IFileStoragePort&>().StoreFile(
                                 std::declval<const FileData&>(),
                                 std::declval<const std::string&>())),
                             Result<std::string>>);
static_assert(std::is_same_v<decltype(std::declval<IFileStoragePort&>().ValidateFile(
                                 std::declval<const FileData&>(),
                                 size_t{},
                                 std::declval<const std::vector<std::string>&>())),
                             Result<void>>);
static_assert(std::is_same_v<decltype(std::declval<IFileStoragePort&>().DeleteFile(std::declval<const std::string&>())),
                             Result<void>>);
static_assert(std::is_same_v<decltype(std::declval<IFileStoragePort&>().FileExists(std::declval<const std::string&>())),
                             Result<bool>>);
static_assert(std::is_same_v<decltype(std::declval<IFileStoragePort&>().GetFileSize(std::declval<const std::string&>())),
                             Result<size_t>>);

TEST(UploadContractsTest, UploadRequestValidateRequiresCanonicalFields) {
    UploadRequest request;
    EXPECT_FALSE(request.Validate());

    request.file_content = {0x30, 0x0A};
    request.original_filename = "fixture.dxf";
    request.file_size = request.file_content.size();
    EXPECT_TRUE(request.Validate());

    request.file_size = 0;
    EXPECT_FALSE(request.Validate());
}

TEST(UploadContractsTest, UploadResponseDefaultsRemainNeutral) {
    const UploadResponse response;

    EXPECT_FALSE(response.success);
    EXPECT_TRUE(response.filepath.empty());
    EXPECT_TRUE(response.original_name.empty());
    EXPECT_EQ(response.size, 0u);
    EXPECT_TRUE(response.generated_filename.empty());
    EXPECT_EQ(response.timestamp, 0);
}

TEST(UploadContractsTest, FileDataCarriesUploadMetadataWithoutTransformation) {
    const FileData file_data{{0x31, 0x32, 0x33}, "fixture.dxf", 3u, "application/dxf"};

    ASSERT_EQ(file_data.content.size(), 3u);
    EXPECT_EQ(file_data.original_name, "fixture.dxf");
    EXPECT_EQ(file_data.size, 3u);
    EXPECT_EQ(file_data.content_type, "application/dxf");
}

}  // namespace
