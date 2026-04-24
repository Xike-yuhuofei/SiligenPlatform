#define MODULE_NAME "VelocityTracePathPolicy"

#include "VelocityTracePathPolicy.h"

#include "shared/interfaces/ILoggingService.h"

#include <iomanip>

namespace Siligen::Application::UseCases::Dispensing {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

namespace {

constexpr const char* kErrorSource = "DispensingExecutionUseCase";

std::string BuildDefaultTraceFilename(const std::string& source_stem) {
    const std::string stem = source_stem.empty() ? "dispensing_execution" : source_stem;
    return stem + "_velocity_trace.csv";
}

Result<std::filesystem::path> WeaklyCanonical(const std::filesystem::path& path) {
    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(path, ec);
    if (ec) {
        return Result<std::filesystem::path>::Failure(
            Error(
                ErrorCode::FILE_PATH_INVALID,
                "failed to normalize path: " + path.string(),
                kErrorSource));
    }
    return Result<std::filesystem::path>::Success(canonical);
}

}  // namespace

Result<std::filesystem::path> VelocityTracePathPolicy::BuildBaseDirectory(const std::string& source_path) {
    if (source_path.empty()) {
        return Result<std::filesystem::path>::Failure(
            Error(
                ErrorCode::INVALID_PARAMETER,
                "velocity trace requires request.source_path or execution_package.source_path",
                kErrorSource));
    }

    std::filesystem::path source(source_path);
    if (source.is_relative()) {
        source = std::filesystem::absolute(source);
    }
    const auto normalized_source = WeaklyCanonical(source);
    if (normalized_source.IsError()) {
        return Result<std::filesystem::path>::Failure(normalized_source.GetError());
    }

    const auto source_dir = normalized_source.Value().parent_path();
    auto base_dir = source_dir / "_runtime-execution" / "velocity-traces";
    return Result<std::filesystem::path>::Success(base_dir);
}

bool VelocityTracePathPolicy::IsWithinBaseDirectory(
    const std::filesystem::path& base_dir,
    const std::filesystem::path& candidate) noexcept {
    auto base_it = base_dir.begin();
    auto candidate_it = candidate.begin();
    for (; base_it != base_dir.end() && candidate_it != candidate.end(); ++base_it, ++candidate_it) {
        if (*base_it != *candidate_it) {
            return false;
        }
    }
    return base_it == base_dir.end();
}

Result<std::filesystem::path> VelocityTracePathPolicy::NormalizeCandidatePath(
    const std::filesystem::path& base_dir,
    const std::string& configured_path,
    const std::string& source_stem) {
    std::filesystem::path configured(configured_path);
    if (configured.is_absolute()) {
        return Result<std::filesystem::path>::Failure(
            Error(
                ErrorCode::FILE_PATH_INVALID,
                "absolute velocity trace path is not allowed",
                kErrorSource));
    }

    std::filesystem::path relative_candidate = configured;
    if (relative_candidate.empty()) {
        relative_candidate = BuildDefaultTraceFilename(source_stem);
    }

    std::filesystem::path filename_candidate = relative_candidate;
    if (filename_candidate.extension().empty()) {
        filename_candidate /= BuildDefaultTraceFilename(source_stem);
    }

    const auto raw_candidate = base_dir / filename_candidate;
    const auto normalized_base = WeaklyCanonical(base_dir);
    if (normalized_base.IsError()) {
        return Result<std::filesystem::path>::Failure(normalized_base.GetError());
    }
    const auto normalized_candidate = WeaklyCanonical(raw_candidate);
    if (normalized_candidate.IsError()) {
        return Result<std::filesystem::path>::Failure(normalized_candidate.GetError());
    }

    if (!IsWithinBaseDirectory(normalized_base.Value(), normalized_candidate.Value())) {
        return Result<std::filesystem::path>::Failure(
            Error(
                ErrorCode::FILE_PATH_INVALID,
                "velocity trace path escapes controlled base directory",
                kErrorSource));
    }

    return Result<std::filesystem::path>::Success(normalized_candidate.Value());
}

Result<std::string> VelocityTracePathPolicy::ResolveOutputPath(
    const std::string& source_path,
    const std::string& configured_path) {
    const auto base_dir = BuildBaseDirectory(source_path);
    if (base_dir.IsError()) {
        return Result<std::string>::Failure(base_dir.GetError());
    }

    std::filesystem::path source(source_path);
    const std::string source_stem = source.stem().string();
    const auto candidate = NormalizeCandidatePath(base_dir.Value(), configured_path, source_stem);
    if (candidate.IsError()) {
        return Result<std::string>::Failure(candidate.GetError());
    }

    return Result<std::string>::Success(candidate.Value().string());
}

Result<void> VelocityTracePathPolicy::PrepareOutputFile(
    const std::string& output_path,
    std::ofstream& file) {
    std::filesystem::path path(output_path);
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        return Result<void>::Failure(
            Error(
                ErrorCode::FILE_IO_ERROR,
                "failed to create velocity trace directory: " + path.parent_path().string(),
                kErrorSource));
    }

    file.open(output_path, std::ios::binary);
    if (!file) {
        return Result<void>::Failure(
            Error(
                ErrorCode::FILE_IO_ERROR,
                "failed to open velocity trace file: " + output_path,
                kErrorSource));
    }

    file.setf(std::ios::fixed);
    file << std::setprecision(6);
    file << "timestamp_s,x_mm,y_mm,velocity_mm_s,dispense_on\n";
    return Result<void>::Success();
}

}  // namespace Siligen::Application::UseCases::Dispensing
