#include "job_ingest/contracts/dispensing/UploadContracts.h"
#include "shared/types/Result.h"

#include <gtest/gtest.h>

#include <type_traits>
#include <utility>

namespace {

using Siligen::JobIngest::Contracts::IUploadFilePort;
using Siligen::JobIngest::Contracts::UploadRequest;
using Siligen::JobIngest::Contracts::UploadResponse;
using Siligen::Shared::Types::Result;

static_assert(std::is_default_constructible_v<UploadRequest>);
static_assert(std::is_default_constructible_v<UploadResponse>);
static_assert(std::is_abstract_v<IUploadFilePort>);
static_assert(std::is_same_v<decltype(std::declval<IUploadFilePort&>().Execute(std::declval<const UploadRequest&>())),
                             Result<UploadResponse>>);

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
    EXPECT_TRUE(response.prepared_filepath.empty());
    EXPECT_TRUE(response.original_name.empty());
    EXPECT_EQ(response.size, 0u);
    EXPECT_TRUE(response.generated_filename.empty());
    EXPECT_EQ(response.timestamp, 0);
    EXPECT_TRUE(response.import_diagnostics.result_classification.empty());
    EXPECT_FALSE(response.import_diagnostics.preview_ready);
    EXPECT_FALSE(response.import_diagnostics.production_ready);
    EXPECT_FALSE(response.import_diagnostics.formal_compare_gate.HasValue());
}

}  // namespace
