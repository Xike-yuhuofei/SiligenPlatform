#include "job_ingest/contracts/dispensing/UploadContracts.h"
#include "shared/types/Result.h"

#include <gtest/gtest.h>

#include <type_traits>
#include <utility>

namespace {

using Siligen::JobIngest::Contracts::IUploadFilePort;
using Siligen::JobIngest::Contracts::SourceDrawing;
using Siligen::JobIngest::Contracts::UploadRequest;
using Siligen::Shared::Types::Result;

static_assert(std::is_default_constructible_v<UploadRequest>);
static_assert(std::is_default_constructible_v<SourceDrawing>);
static_assert(std::is_abstract_v<IUploadFilePort>);
static_assert(std::is_same_v<decltype(std::declval<IUploadFilePort&>().Execute(std::declval<const UploadRequest&>())),
                             Result<SourceDrawing>>);

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

TEST(UploadContractsTest, SourceDrawingDefaultsRemainNeutral) {
    const SourceDrawing response;

    EXPECT_TRUE(response.source_drawing_ref.empty());
    EXPECT_TRUE(response.filepath.empty());
    EXPECT_TRUE(response.original_name.empty());
    EXPECT_EQ(response.size, 0u);
    EXPECT_TRUE(response.generated_filename.empty());
    EXPECT_EQ(response.timestamp, 0);
    EXPECT_TRUE(response.source_hash.empty());
    EXPECT_EQ(response.validation_report.schema_version, "DXFValidationReport.v1");
    EXPECT_EQ(response.validation_report.gate_result, "PASS");
    EXPECT_FALSE(response.validation_report.preview_ready);
    EXPECT_FALSE(response.validation_report.production_ready);
}

}  // namespace
