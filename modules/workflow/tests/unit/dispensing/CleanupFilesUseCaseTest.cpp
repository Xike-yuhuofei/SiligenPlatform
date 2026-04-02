#include "application/usecases/dispensing/CleanupFilesUseCase.h"
#include "job_ingest/contracts/storage/IFileStoragePort.h"
#include "shared/types/Error.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

using Siligen::Application::UseCases::Dispensing::CleanupFilesRequest;
using Siligen::Application::UseCases::Dispensing::CleanupFilesUseCase;
using Siligen::JobIngest::Contracts::Storage::FileData;
using Siligen::JobIngest::Contracts::Storage::IFileStoragePort;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

class TestFileStoragePort final : public IFileStoragePort {
   public:
    Result<std::string> StoreFile(const FileData&, const std::string&) override {
        return Result<std::string>::Failure(
            Error(ErrorCode::NOT_IMPLEMENTED, "StoreFile not used", "CleanupFilesUseCaseTest"));
    }

    Result<void> ValidateFile(const FileData&, size_t, const std::vector<std::string>&) override {
        return Result<void>::Success();
    }

    Result<void> DeleteFile(const std::string& filepath) override {
        deleted_files.push_back(filepath);
        std::error_code ec;
        std::filesystem::remove(filepath, ec);
        if (ec) {
            return Result<void>::Failure(
                Error(ErrorCode::FILE_IO_ERROR, "delete failed", "CleanupFilesUseCaseTest"));
        }
        return Result<void>::Success();
    }

    Result<bool> FileExists(const std::string& filepath) override {
        return Result<bool>::Success(std::filesystem::exists(filepath));
    }

    Result<size_t> GetFileSize(const std::string& filepath) override {
        std::error_code ec;
        const auto size = std::filesystem::file_size(filepath, ec);
        if (ec) {
            return Result<size_t>::Failure(
                Error(ErrorCode::FILE_IO_ERROR, "size query failed", "CleanupFilesUseCaseTest"));
        }
        return Result<size_t>::Success(static_cast<size_t>(size));
    }

    std::vector<std::string> deleted_files;
};

std::filesystem::path MakeTempDir(const std::string& suffix) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path = std::filesystem::temp_directory_path() / ("siligen_cleanup_dxf_" + suffix + "_" + std::to_string(now));
    std::filesystem::create_directories(path);
    return path;
}

std::filesystem::path WriteFile(const std::filesystem::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    out << content;
    out.close();
    return path;
}

void SetFileAgeHours(const std::filesystem::path& path, int hours_ago) {
    const auto now = std::filesystem::file_time_type::clock::now();
    std::filesystem::last_write_time(path, now - std::chrono::hours(hours_ago));
}

}  // namespace

TEST(CleanupFilesUseCaseTest, DeletesExpiredDxfFilesOnly) {
    const auto base_dir = MakeTempDir("delete_expired");
    const auto expired_dxf = WriteFile(base_dir / "expired.dxf", "old");
    const auto fresh_dxf = WriteFile(base_dir / "fresh.dxf", "fresh");
    const auto txt_file = WriteFile(base_dir / "note.txt", "ignore");
    SetFileAgeHours(expired_dxf, 48);
    SetFileAgeHours(fresh_dxf, 1);
    SetFileAgeHours(txt_file, 72);

    auto storage = std::make_shared<TestFileStoragePort>();
    CleanupFilesUseCase use_case(storage, base_dir.string());

    CleanupFilesRequest request;
    request.max_age_hours = 24;
    request.dry_run = false;

    auto result = use_case.Execute(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();
    EXPECT_EQ(result.Value().deleted_count, 1u);
    ASSERT_EQ(result.Value().deleted_files.size(), 1u);
    EXPECT_EQ(result.Value().deleted_files.front(), expired_dxf.string());
    EXPECT_FALSE(std::filesystem::exists(expired_dxf));
    EXPECT_TRUE(std::filesystem::exists(fresh_dxf));
    EXPECT_TRUE(std::filesystem::exists(txt_file));

    std::error_code ec;
    std::filesystem::remove_all(base_dir, ec);
}

TEST(CleanupFilesUseCaseTest, DryRunReportsDeletionWithoutRemovingFiles) {
    const auto base_dir = MakeTempDir("dry_run");
    const auto expired_dxf = WriteFile(base_dir / "expired.dxf", "old");
    SetFileAgeHours(expired_dxf, 48);

    auto storage = std::make_shared<TestFileStoragePort>();
    CleanupFilesUseCase use_case(storage, base_dir.string());

    CleanupFilesRequest request;
    request.max_age_hours = 24;
    request.dry_run = true;

    auto result = use_case.Execute(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();
    EXPECT_EQ(result.Value().deleted_count, 1u);
    EXPECT_TRUE(std::filesystem::exists(expired_dxf));
    EXPECT_TRUE(storage->deleted_files.empty());

    std::error_code ec;
    std::filesystem::remove_all(base_dir, ec);
}

TEST(CleanupFilesUseCaseTest, SkipsFilesReportedAsInUse) {
    const auto base_dir = MakeTempDir("in_use");
    const auto expired_dxf = WriteFile(base_dir / "busy.dxf", "old");
    SetFileAgeHours(expired_dxf, 72);

    auto storage = std::make_shared<TestFileStoragePort>();
    CleanupFilesUseCase use_case(
        storage,
        base_dir.string(),
        [target = expired_dxf.string()](const std::string& filepath) { return filepath == target; });

    CleanupFilesRequest request;
    request.max_age_hours = 24;

    auto result = use_case.Execute(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();
    EXPECT_EQ(result.Value().deleted_count, 0u);
    EXPECT_TRUE(std::filesystem::exists(expired_dxf));
    EXPECT_TRUE(storage->deleted_files.empty());

    std::error_code ec;
    std::filesystem::remove_all(base_dir, ec);
}


