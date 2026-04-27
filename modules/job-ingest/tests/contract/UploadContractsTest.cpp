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
    const SourceDrawing drawing;

    EXPECT_TRUE(drawing.source_drawing_ref.empty());
    EXPECT_TRUE(drawing.filepath.empty());
    EXPECT_TRUE(drawing.original_name.empty());
    EXPECT_EQ(drawing.size, 0u);
    EXPECT_TRUE(drawing.generated_filename.empty());
    EXPECT_EQ(drawing.timestamp, 0);
    EXPECT_TRUE(drawing.source_hash.empty());
    EXPECT_EQ(drawing.validation_report.schema_version, "DXFValidationReport.v1");
    EXPECT_TRUE(drawing.validation_report.stage_id.empty());
    EXPECT_TRUE(drawing.validation_report.owner_module.empty());
    EXPECT_FALSE(drawing.validation_report.preview_ready);
    EXPECT_FALSE(drawing.validation_report.production_ready);
    EXPECT_FALSE(drawing.validation_report.formal_compare_gate.has_value);
}

}  // namespace
