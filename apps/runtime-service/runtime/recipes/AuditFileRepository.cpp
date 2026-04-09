#include "AuditFileRepository.h"

#include "recipe_lifecycle/adapters/serialization/RecipeJsonSerializer.h"
#include "shared/types/Error.h"
#include "shared/interfaces/ILoggingService.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "AuditFileRepository"

namespace {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Siligen::Domain::Recipes::Serialization::RecipeJsonSerializer;
using Siligen::Domain::Recipes::ValueObjects::AuditRecord;
using Siligen::Domain::Recipes::ValueObjects::RecipeId;
using Siligen::Domain::Recipes::ValueObjects::RecipeVersionId;

std::string GenerateId(const std::string& prefix) {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<std::uint64_t> dist;

    std::ostringstream oss;
    oss << prefix << "_" << now << "_" << std::hex << dist(gen);
    return oss.str();
}

std::int64_t NowTimestamp() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

Result<std::string> ReadFile(const std::filesystem::path& filepath) {
    std::ifstream file(filepath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return Result<std::string>::Failure(
            Error(ErrorCode::FILE_NOT_FOUND, "File not found: " + filepath.string(), MODULE_NAME));
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return Result<std::string>::Success(buffer.str());
}

Result<std::vector<AuditRecord>> LoadRecords(const std::filesystem::path& filepath) {
    std::vector<AuditRecord> records;

    std::error_code ec;
    if (!std::filesystem::exists(filepath, ec) || ec) {
        return Result<std::vector<AuditRecord>>::Success(records);
    }

    auto content = ReadFile(filepath);
    if (content.IsError()) {
        return Result<std::vector<AuditRecord>>::Failure(content.GetError());
    }

    auto json_result = RecipeJsonSerializer::Parse(content.Value());
    if (json_result.IsError()) {
        return Result<std::vector<AuditRecord>>::Failure(json_result.GetError());
    }

    const auto& root = json_result.Value();
    if (root.contains("records") && root.at("records").is_array()) {
        for (const auto& item : root.at("records")) {
            auto record_result = RecipeJsonSerializer::AuditRecordFromJson(item);
            if (record_result.IsError()) {
                return Result<std::vector<AuditRecord>>::Failure(record_result.GetError());
            }
            records.push_back(record_result.Value());
        }
    }

    return Result<std::vector<AuditRecord>>::Success(records);
}

Result<void> WriteRecords(const std::filesystem::path& filepath, const std::vector<AuditRecord>& records) {
    RecipeJsonSerializer::json root;
    root["schemaVersion"] = "1.0";

    RecipeJsonSerializer::json output = RecipeJsonSerializer::json::array();
    for (const auto& record : records) {
        output.push_back(RecipeJsonSerializer::AuditRecordToJson(record));
    }
    root["records"] = output;

    auto dump_result = RecipeJsonSerializer::Dump(root);
    if (dump_result.IsError()) {
        return Result<void>::Failure(dump_result.GetError());
    }

    std::ofstream file(filepath, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return Result<void>::Failure(
            Error(ErrorCode::FILE_IO_ERROR, "Unable to write file: " + filepath.string(), MODULE_NAME));
    }

    file << dump_result.Value();
    if (!file.good()) {
        return Result<void>::Failure(
            Error(ErrorCode::FILE_IO_ERROR, "Write failed for file: " + filepath.string(), MODULE_NAME));
    }

    return Result<void>::Success();
}

}  // namespace

namespace Siligen::Infrastructure::Adapters::Recipes {

AuditFileRepository::AuditFileRepository(const std::string& base_directory)
    : base_directory_(base_directory),
      audit_directory_(std::filesystem::path(base_directory_) / "audit") {
    auto ensure_result = EnsureDirectory();
    if (ensure_result.IsError()) {
        SILIGEN_LOG_WARNING("Failed to ensure audit directory: " + ensure_result.GetError().GetMessage());
    }
}

Result<void> AuditFileRepository::EnsureDirectory() const {
    std::error_code ec;
    std::filesystem::create_directories(audit_directory_, ec);
    if (ec) {
        return Result<void>::Failure(Error(ErrorCode::FILE_IO_ERROR,
                                           "Failed to create directory: " + audit_directory_.string() + ", " + ec.message(),
                                           MODULE_NAME));
    }
    return Result<void>::Success();
}

std::filesystem::path AuditFileRepository::AuditPath(const RecipeId& recipe_id) const {
    return audit_directory_ / (recipe_id + ".json");
}

Result<void> AuditFileRepository::AppendRecord(const AuditRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto ensure_result = EnsureDirectory();
    if (ensure_result.IsError()) {
        return Result<void>::Failure(ensure_result.GetError());
    }

    auto filepath = AuditPath(record.recipe_id);
    auto records_result = LoadRecords(filepath);
    if (records_result.IsError()) {
        return Result<void>::Failure(records_result.GetError());
    }

    auto records = records_result.Value();
    AuditRecord updated = record;

    if (updated.id.empty()) {
        updated.id = GenerateId("audit");
    }
    if (updated.timestamp == 0) {
        updated.timestamp = NowTimestamp();
    }

    records.push_back(updated);

    auto write_result = WriteRecords(filepath, records);
    if (write_result.IsError()) {
        return Result<void>::Failure(write_result.GetError());
    }

    return Result<void>::Success();
}

Result<std::vector<AuditRecord>> AuditFileRepository::ListByRecipe(const RecipeId& recipe_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto records_result = LoadRecords(AuditPath(recipe_id));
    if (records_result.IsError()) {
        return Result<std::vector<AuditRecord>>::Failure(records_result.GetError());
    }

    return Result<std::vector<AuditRecord>>::Success(records_result.Value());
}

Result<std::vector<AuditRecord>> AuditFileRepository::ListByVersion(const RecipeId& recipe_id,
                                                                     const RecipeVersionId& version_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto records_result = LoadRecords(AuditPath(recipe_id));
    if (records_result.IsError()) {
        return Result<std::vector<AuditRecord>>::Failure(records_result.GetError());
    }

    std::vector<AuditRecord> result;
    for (const auto& record : records_result.Value()) {
        if (record.recipe_id == recipe_id && record.version_id.has_value() &&
            record.version_id.value() == version_id) {
            result.push_back(record);
        }
    }

    return Result<std::vector<AuditRecord>>::Success(result);
}

}  // namespace Siligen::Infrastructure::Adapters::Recipes
