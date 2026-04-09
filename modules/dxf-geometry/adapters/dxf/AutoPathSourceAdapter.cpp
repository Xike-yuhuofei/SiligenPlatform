#include "dxf_geometry/adapters/planning/dxf/AutoPathSourceAdapter.h"

#include "shared/types/Error.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace Siligen::Infrastructure::Adapters::Parsing {

namespace {

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::tolower(c); });
    return value;
}

bool EndsWith(const std::string& value, const std::string& suffix) {
    if (value.size() < suffix.size()) {
        return false;
    }
    return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

Siligen::Shared::Types::Error DirectDxfRequiresPbError(const std::string& filepath) {
    const std::string target = filepath.empty() ? std::string("DXF") : filepath;
    return Siligen::Shared::Types::Error(
        Siligen::Shared::Types::ErrorCode::FILE_FORMAT_INVALID,
        "DXF 不是 AutoPathSourceAdapter 的直接输入，请先通过 DxfPbPreparationService 生成 .pb: " + target,
        "AutoPathSourceAdapter");
}

Siligen::Shared::Types::Error UnsupportedPathSourceError(const std::string& filepath) {
    return Siligen::Shared::Types::Error(
        Siligen::Shared::Types::ErrorCode::FILE_FORMAT_INVALID,
        "仅支持 .pb 路径输入；DXF 需先经 DxfPbPreparationService 生成 .pb: " + filepath,
        "AutoPathSourceAdapter");
}

}  // namespace

AutoPathSourceAdapter::AutoPathSourceAdapter() : pb_adapter_(std::make_shared<PbPathSourceAdapter>()) {
}

Result<PathSourceResult> AutoPathSourceAdapter::LoadFromFile(const std::string& filepath) {
    const std::string lower = ToLower(filepath);

    if (EndsWith(lower, ".pb")) {
        return pb_adapter_->LoadFromFile(filepath);
    }

    if (EndsWith(lower, ".dxf")) {
        return Result<PathSourceResult>::Failure(DirectDxfRequiresPbError(filepath));
    }

    return Result<PathSourceResult>::Failure(UnsupportedPathSourceError(filepath));
}

Result<DXFPathSourceResult> AutoPathSourceAdapter::LoadDXFFile(const std::string& filepath) {
    return Result<DXFPathSourceResult>::Failure(DirectDxfRequiresPbError(filepath));
}

Result<DXFValidationResult> AutoPathSourceAdapter::ValidateDXFFile(const std::string& filepath) {
    const std::string lower = ToLower(filepath);
    if (!EndsWith(lower, ".dxf")) {
        return Result<DXFValidationResult>::Failure(
            Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::FILE_FORMAT_INVALID,
                                          "仅支持DXF文件校验: " + filepath,
                                          "AutoPathSourceAdapter"));
    }

    if (!std::filesystem::exists(filepath)) {
        return Result<DXFValidationResult>::Failure(
            Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::FILE_NOT_FOUND,
                                          "DXF文件不存在: " + filepath,
                                          "AutoPathSourceAdapter"));
    }

    DXFValidationResult result;
    result.is_valid = true;
    result.format_version = "DXF (canonical .pb preprocessing required)";
    result.entity_count = 0;
    result.requires_preprocessing = true;
    return Result<DXFValidationResult>::Success(result);
}

std::vector<std::string> AutoPathSourceAdapter::GetSupportedFormats() {
    return {"PB", "DXF (requires canonical .pb preprocessing)"};
}

Result<bool> AutoPathSourceAdapter::RequiresPreprocessing(const std::string& filepath) {
    const std::string lower = ToLower(filepath);
    if (EndsWith(lower, ".pb")) {
        return Result<bool>::Success(false);
    }

    if (!EndsWith(lower, ".dxf")) {
        return Result<bool>::Failure(UnsupportedPathSourceError(filepath));
    }

    if (!std::filesystem::exists(filepath)) {
        return Result<bool>::Failure(
            Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::FILE_NOT_FOUND,
                                          "DXF文件不存在: " + filepath,
                                          "AutoPathSourceAdapter"));
    }

    return Result<bool>::Success(true);
}

}  // namespace Siligen::Infrastructure::Adapters::Parsing
