#include "infrastructure/adapters/redundancy/JsonRedundancyRepositoryAdapter.h"

#include "shared/types/Error.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <unordered_map>

namespace Siligen::Infrastructure::Adapters::Redundancy {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Domain::System::Redundancy::CandidateActionFromString;
using Domain::System::Redundancy::CandidatePriorityFromString;
using Domain::System::Redundancy::CandidateStatus;
using Domain::System::Redundancy::CandidateStatusFromString;
using Domain::System::Redundancy::DecisionActionFromString;
using Domain::System::Redundancy::EntityTypeFromString;
using Domain::System::Redundancy::RuntimeEnvironmentFromString;
using Domain::System::Redundancy::SourceLanguageFromString;
using Domain::System::Redundancy::StaticSignalTypeFromString;
using Domain::System::Redundancy::ToString;
using Domain::System::Redundancy::Ports::BatchWriteSummary;
using Domain::System::Redundancy::Ports::CandidateQueryFilter;
using Domain::System::Redundancy::Ports::CandidateTransitionRequest;

namespace {

constexpr const char* kStoreVersion = "1.0.0";
constexpr const char* kStoreKeyVersion = "version";
constexpr const char* kStoreKeyItems = "items";

std::filesystem::path ResolveDefaultBaseDir() {
    return std::filesystem::path("packages") / "process-runtime-core" / "tmp" / "redundancy-governance";
}

Result<void> EnsureFileInitialized(const std::filesystem::path& filepath) {
    if (std::filesystem::exists(filepath)) {
        return Result<void>::Success();
    }
    std::filesystem::create_directories(filepath.parent_path());
    nlohmann::json payload = {{kStoreKeyVersion, kStoreVersion}, {kStoreKeyItems, nlohmann::json::array()}};

    std::ofstream out(filepath, std::ios::binary);
    if (!out.is_open()) {
        return Result<void>::Failure(
            Error(ErrorCode::FILE_IO_ERROR, "Failed to initialize json store file: " + filepath.string(), "JsonRedundancyRepositoryAdapter"));
    }
    out << payload.dump(2);
    return Result<void>::Success();
}

Result<nlohmann::json> LoadStore(const std::filesystem::path& filepath) {
    std::ifstream in(filepath, std::ios::binary);
    if (!in.is_open()) {
        auto backup_path = filepath;
        backup_path += ".bak";
        if (std::filesystem::exists(backup_path)) {
            std::error_code recover_ec;
            std::filesystem::copy_file(
                backup_path,
                filepath,
                std::filesystem::copy_options::overwrite_existing,
                recover_ec);
            std::ifstream recovered_in(filepath, std::ios::binary);
            if (recovered_in.is_open()) {
                in.swap(recovered_in);
            }
        }
    }

    if (!in.is_open()) {
        return Result<nlohmann::json>::Failure(
            Error(ErrorCode::FILE_IO_ERROR, "Failed to open store file: " + filepath.string(), "JsonRedundancyRepositoryAdapter"));
    }

    nlohmann::json payload;
    try {
        in >> payload;
    } catch (const std::exception& ex) {
        return Result<nlohmann::json>::Failure(
            Error(ErrorCode::JSON_PARSE_ERROR, std::string("Invalid json store file: ") + ex.what(), "JsonRedundancyRepositoryAdapter"));
    }

    if (!payload.is_object() || !payload.contains(kStoreKeyItems) || !payload[kStoreKeyItems].is_array()) {
        return Result<nlohmann::json>::Failure(
            Error(ErrorCode::JSON_INVALID_TYPE, "Store payload must contain array field 'items'", "JsonRedundancyRepositoryAdapter"));
    }
    return Result<nlohmann::json>::Success(payload);
}

Result<void> SaveStoreAtomically(const std::filesystem::path& filepath, const nlohmann::json& payload) {
    std::filesystem::create_directories(filepath.parent_path());
    auto tmp_path = filepath;
    tmp_path += ".tmp";
    auto backup_path = filepath;
    backup_path += ".bak";

    std::ofstream out(tmp_path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return Result<void>::Failure(
            Error(ErrorCode::FILE_IO_ERROR, "Failed to open temp file for store: " + tmp_path.string(), "JsonRedundancyRepositoryAdapter"));
    }
    out << payload.dump(2);
    out.close();

    std::error_code ec;
    const bool had_original = std::filesystem::exists(filepath);
    if (had_original) {
        std::filesystem::remove(backup_path, ec);
        ec.clear();
        std::filesystem::copy_file(
            filepath,
            backup_path,
            std::filesystem::copy_options::overwrite_existing,
            ec);
        if (ec) {
            std::filesystem::remove(tmp_path, ec);
            return Result<void>::Failure(
                Error(
                    ErrorCode::FILE_IO_ERROR,
                    "failure_stage=create_backup;failure_code=FILE_IO_ERROR;message=" + ec.message(),
                    "JsonRedundancyRepositoryAdapter"));
        }
    }

    ec.clear();
    std::filesystem::rename(tmp_path, filepath, ec);
    if (ec && had_original) {
        ec.clear();
        std::filesystem::remove(filepath, ec);
        ec.clear();
        std::filesystem::rename(tmp_path, filepath, ec);
    }
    if (ec) {
        std::error_code rollback_ec;
        if (had_original && std::filesystem::exists(backup_path)) {
            std::filesystem::copy_file(
                backup_path,
                filepath,
                std::filesystem::copy_options::overwrite_existing,
                rollback_ec);
        }
        std::error_code cleanup_ec;
        std::filesystem::remove(tmp_path, cleanup_ec);

        return Result<void>::Failure(
            Error(
                ErrorCode::FILE_IO_ERROR,
                "failure_stage=replace_store;failure_code=FILE_IO_ERROR;message=" + ec.message() +
                    ";rollback_status=" + (rollback_ec ? rollback_ec.message() : "success"),
                "JsonRedundancyRepositoryAdapter"));
    }

    if (had_original) {
        std::filesystem::remove(backup_path, ec);
    }
    return Result<void>::Success();
}

bool WindowOverlaps(const std::string& left_start,
                    const std::string& left_end,
                    const std::string& right_start,
                    const std::string& right_end) {
    if (left_start.empty() || left_end.empty() || right_start.empty() || right_end.empty()) {
        return true;
    }
    return left_start <= right_end && right_start <= left_end;
}

template <typename Record, typename Parser>
Result<std::vector<Record>> LoadRecords(const std::filesystem::path& filepath, Parser parser) {
    auto store_result = LoadStore(filepath);
    if (store_result.IsError()) {
        return Result<std::vector<Record>>::Failure(store_result.GetError());
    }

    std::vector<Record> records;
    for (const auto& item : store_result.Value().at(kStoreKeyItems)) {
        auto parsed = parser(item);
        if (parsed.IsError()) {
            return Result<std::vector<Record>>::Failure(parsed.GetError());
        }
        records.emplace_back(parsed.Value());
    }
    return Result<std::vector<Record>>::Success(std::move(records));
}

template <typename Record, typename Writer>
Result<void> SaveRecords(const std::filesystem::path& filepath, const std::vector<Record>& records, Writer writer) {
    nlohmann::json payload = {{kStoreKeyVersion, kStoreVersion}, {kStoreKeyItems, nlohmann::json::array()}};
    for (const auto& record : records) {
        payload[kStoreKeyItems].push_back(writer(record));
    }
    return SaveStoreAtomically(filepath, payload);
}

template <typename Record, typename IdExtractor>
std::size_t UpsertById(std::vector<Record>& existing, const std::vector<Record>& incoming, IdExtractor id_extractor) {
    std::unordered_map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < existing.size(); ++i) {
        index[id_extractor(existing[i])] = i;
    }

    std::size_t upserted = 0;
    for (const auto& record : incoming) {
        const auto& id = id_extractor(record);
        auto it = index.find(id);
        if (it == index.end()) {
            index[id] = existing.size();
            existing.push_back(record);
        } else {
            existing[it->second] = record;
        }
        ++upserted;
    }
    return upserted;
}

nlohmann::json ToJson(const Domain::System::Redundancy::CodeEntity& entity) {
    return {
        {"entity_id", entity.entity_id},
        {"entity_type", ToString(entity.entity_type)},
        {"symbol", entity.symbol},
        {"path", entity.path},
        {"signature", entity.signature},
        {"module", entity.module},
        {"owner_target", entity.owner_target},
        {"language", ToString(entity.language)},
        {"last_modified_at", entity.last_modified_at}
    };
}

Result<Domain::System::Redundancy::CodeEntity> ParseCodeEntity(const nlohmann::json& value) {
    Domain::System::Redundancy::CodeEntity entity;
    if (!value.is_object()) {
        return Result<Domain::System::Redundancy::CodeEntity>::Failure(
            Error(ErrorCode::JSON_INVALID_TYPE, "CodeEntity must be object", "JsonRedundancyRepositoryAdapter"));
    }
    entity.entity_id = value.value("entity_id", "");
    auto entity_type = EntityTypeFromString(value.value("entity_type", ""));
    auto language = SourceLanguageFromString(value.value("language", ""));
    if (!entity_type.has_value() || !language.has_value()) {
        return Result<Domain::System::Redundancy::CodeEntity>::Failure(
            Error(ErrorCode::JSON_INVALID_TYPE, "CodeEntity enum conversion failed", "JsonRedundancyRepositoryAdapter"));
    }
    entity.entity_type = entity_type.value();
    entity.symbol = value.value("symbol", "");
    entity.path = value.value("path", "");
    entity.signature = value.value("signature", "");
    entity.module = value.value("module", "");
    entity.owner_target = value.value("owner_target", "");
    entity.language = language.value();
    entity.last_modified_at = value.value("last_modified_at", "");
    if (!entity.Validate()) {
        return Result<Domain::System::Redundancy::CodeEntity>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "CodeEntity validation failed", "JsonRedundancyRepositoryAdapter"));
    }
    return Result<Domain::System::Redundancy::CodeEntity>::Success(entity);
}

nlohmann::json ToJson(const Domain::System::Redundancy::StaticEvidenceRecord& record) {
    return {
        {"evidence_id", record.evidence_id},
        {"entity_id", record.entity_id},
        {"rule_id", record.rule_id},
        {"rule_version", record.rule_version},
        {"signal_type", ToString(record.signal_type)},
        {"signal_strength", record.signal_strength},
        {"details_json", record.details_json},
        {"collected_at", record.collected_at}
    };
}

Result<Domain::System::Redundancy::StaticEvidenceRecord> ParseStaticEvidence(const nlohmann::json& value) {
    Domain::System::Redundancy::StaticEvidenceRecord record;
    if (!value.is_object()) {
        return Result<Domain::System::Redundancy::StaticEvidenceRecord>::Failure(
            Error(ErrorCode::JSON_INVALID_TYPE, "StaticEvidenceRecord must be object", "JsonRedundancyRepositoryAdapter"));
    }
    record.evidence_id = value.value("evidence_id", "");
    record.entity_id = value.value("entity_id", "");
    record.rule_id = value.value("rule_id", "");
    record.rule_version = value.value("rule_version", "");
    auto signal_type = StaticSignalTypeFromString(value.value("signal_type", ""));
    if (!signal_type.has_value()) {
        return Result<Domain::System::Redundancy::StaticEvidenceRecord>::Failure(
            Error(ErrorCode::JSON_INVALID_TYPE, "StaticEvidence signal_type conversion failed", "JsonRedundancyRepositoryAdapter"));
    }
    record.signal_type = signal_type.value();
    record.signal_strength = value.value("signal_strength", 0.0);
    record.details_json = value.value("details_json", "{}");
    record.collected_at = value.value("collected_at", "");
    if (!record.Validate()) {
        return Result<Domain::System::Redundancy::StaticEvidenceRecord>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "StaticEvidenceRecord validation failed", "JsonRedundancyRepositoryAdapter"));
    }
    return Result<Domain::System::Redundancy::StaticEvidenceRecord>::Success(record);
}

nlohmann::json ToJson(const Domain::System::Redundancy::RuntimeEvidenceRecord& record) {
    return {
        {"evidence_id", record.evidence_id},
        {"entity_id", record.entity_id},
        {"environment", ToString(record.environment)},
        {"window_start", record.window_start},
        {"window_end", record.window_end},
        {"hit_count", record.hit_count},
        {"last_hit_at", record.last_hit_at},
        {"source", record.source},
        {"collected_at", record.collected_at}
    };
}

Result<Domain::System::Redundancy::RuntimeEvidenceRecord> ParseRuntimeEvidence(const nlohmann::json& value) {
    Domain::System::Redundancy::RuntimeEvidenceRecord record;
    if (!value.is_object()) {
        return Result<Domain::System::Redundancy::RuntimeEvidenceRecord>::Failure(
            Error(ErrorCode::JSON_INVALID_TYPE, "RuntimeEvidenceRecord must be object", "JsonRedundancyRepositoryAdapter"));
    }
    record.evidence_id = value.value("evidence_id", "");
    record.entity_id = value.value("entity_id", "");
    auto environment = RuntimeEnvironmentFromString(value.value("environment", ""));
    if (!environment.has_value()) {
        return Result<Domain::System::Redundancy::RuntimeEvidenceRecord>::Failure(
            Error(ErrorCode::JSON_INVALID_TYPE, "RuntimeEvidence environment conversion failed", "JsonRedundancyRepositoryAdapter"));
    }
    record.environment = environment.value();
    record.window_start = value.value("window_start", "");
    record.window_end = value.value("window_end", "");
    record.hit_count = value.value("hit_count", 0LL);
    record.last_hit_at = value.value("last_hit_at", "");
    record.source = value.value("source", "");
    record.collected_at = value.value("collected_at", "");
    if (!record.Validate()) {
        return Result<Domain::System::Redundancy::RuntimeEvidenceRecord>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "RuntimeEvidenceRecord validation failed", "JsonRedundancyRepositoryAdapter"));
    }
    return Result<Domain::System::Redundancy::RuntimeEvidenceRecord>::Success(record);
}

nlohmann::json ToJson(const Domain::System::Redundancy::CandidateRecord& record) {
    return {
        {"candidate_id", record.candidate_id},
        {"entity_id", record.entity_id},
        {"redundancy_score", record.redundancy_score},
        {"deletion_risk_score", record.deletion_risk_score},
        {"priority", ToString(record.priority)},
        {"recommended_action", ToString(record.recommended_action)},
        {"status", ToString(record.status)},
        {"policy_version", record.policy_version},
        {"snapshot_id", record.snapshot_id},
        {"computed_at", record.computed_at}
    };
}

Result<Domain::System::Redundancy::CandidateRecord> ParseCandidateRecord(const nlohmann::json& value) {
    Domain::System::Redundancy::CandidateRecord record;
    if (!value.is_object()) {
        return Result<Domain::System::Redundancy::CandidateRecord>::Failure(
            Error(ErrorCode::JSON_INVALID_TYPE, "CandidateRecord must be object", "JsonRedundancyRepositoryAdapter"));
    }
    record.candidate_id = value.value("candidate_id", "");
    record.entity_id = value.value("entity_id", "");
    record.redundancy_score = value.value("redundancy_score", 0.0);
    record.deletion_risk_score = value.value("deletion_risk_score", 0.0);
    auto priority = CandidatePriorityFromString(value.value("priority", ""));
    auto action = CandidateActionFromString(value.value("recommended_action", ""));
    auto status = CandidateStatusFromString(value.value("status", ""));
    if (!priority.has_value() || !action.has_value() || !status.has_value()) {
        return Result<Domain::System::Redundancy::CandidateRecord>::Failure(
            Error(ErrorCode::JSON_INVALID_TYPE, "CandidateRecord enum conversion failed", "JsonRedundancyRepositoryAdapter"));
    }
    record.priority = priority.value();
    record.recommended_action = action.value();
    record.status = status.value();
    record.policy_version = value.value("policy_version", "");
    record.snapshot_id = value.value("snapshot_id", "");
    record.computed_at = value.value("computed_at", "");
    if (!record.Validate()) {
        return Result<Domain::System::Redundancy::CandidateRecord>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "CandidateRecord validation failed", "JsonRedundancyRepositoryAdapter"));
    }
    return Result<Domain::System::Redundancy::CandidateRecord>::Success(record);
}

nlohmann::json ToJson(const Domain::System::Redundancy::DecisionLogRecord& record) {
    return {
        {"decision_id", record.decision_id},
        {"candidate_id", record.candidate_id},
        {"action", ToString(record.action)},
        {"actor", record.actor},
        {"reason", record.reason},
        {"evidence_snapshot_json", record.evidence_snapshot_json},
        {"ticket", record.ticket},
        {"created_at", record.created_at}
    };
}

std::string CanonicalizeDecisionSnapshotJson(const std::string& snapshot_json) {
    if (snapshot_json.empty()) {
        return "{}";
    }
    try {
        return nlohmann::json::parse(snapshot_json).dump();
    } catch (...) {
        return snapshot_json;
    }
}

bool IsEquivalentDecisionLogRecord(const Domain::System::Redundancy::DecisionLogRecord& left,
                                   const Domain::System::Redundancy::DecisionLogRecord& right) {
    return left.decision_id == right.decision_id &&
           left.candidate_id == right.candidate_id &&
           left.action == right.action &&
           left.actor == right.actor &&
           left.reason == right.reason &&
           CanonicalizeDecisionSnapshotJson(left.evidence_snapshot_json) ==
               CanonicalizeDecisionSnapshotJson(right.evidence_snapshot_json) &&
           left.ticket == right.ticket &&
           left.created_at == right.created_at;
}

Result<Domain::System::Redundancy::DecisionLogRecord> ParseDecisionLogRecord(const nlohmann::json& value) {
    Domain::System::Redundancy::DecisionLogRecord record;
    if (!value.is_object()) {
        return Result<Domain::System::Redundancy::DecisionLogRecord>::Failure(
            Error(ErrorCode::JSON_INVALID_TYPE, "DecisionLogRecord must be object", "JsonRedundancyRepositoryAdapter"));
    }
    record.decision_id = value.value("decision_id", "");
    record.candidate_id = value.value("candidate_id", "");
    auto action = DecisionActionFromString(value.value("action", ""));
    if (!action.has_value()) {
        return Result<Domain::System::Redundancy::DecisionLogRecord>::Failure(
            Error(ErrorCode::JSON_INVALID_TYPE, "DecisionLog action conversion failed", "JsonRedundancyRepositoryAdapter"));
    }
    record.action = action.value();
    record.actor = value.value("actor", "");
    record.reason = value.value("reason", "");
    record.evidence_snapshot_json = value.value("evidence_snapshot_json", "{}");
    record.ticket = value.value("ticket", "");
    record.created_at = value.value("created_at", "");
    if (!record.Validate()) {
        return Result<Domain::System::Redundancy::DecisionLogRecord>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "DecisionLogRecord validation failed", "JsonRedundancyRepositoryAdapter"));
    }
    return Result<Domain::System::Redundancy::DecisionLogRecord>::Success(record);
}

std::unordered_map<std::string, std::string> BuildEntityModuleMap(
    const std::vector<Domain::System::Redundancy::CodeEntity>& entities) {
    std::unordered_map<std::string, std::string> module_map;
    for (const auto& entity : entities) {
        module_map[entity.entity_id] = entity.module;
    }
    return module_map;
}

}  // namespace

JsonRedundancyRepositoryAdapter::JsonRedundancyRepositoryAdapter(std::filesystem::path base_dir)
    : base_dir_(base_dir.empty() ? ResolveDefaultBaseDir() : std::move(base_dir)),
      entities_file_(base_dir_ / "code_entities.json"),
      static_evidence_file_(base_dir_ / "static_evidence.json"),
      runtime_evidence_file_(base_dir_ / "runtime_evidence.json"),
      candidates_file_(base_dir_ / "candidates.json"),
      decision_logs_file_(base_dir_ / "decision_logs.json") {
    EnsureStoreInitialized();
}

Result<void> JsonRedundancyRepositoryAdapter::EnsureStoreInitialized() const {
    auto initialized_entities = EnsureFileInitialized(entities_file_);
    if (initialized_entities.IsError()) {
        return initialized_entities;
    }
    auto initialized_static = EnsureFileInitialized(static_evidence_file_);
    if (initialized_static.IsError()) {
        return initialized_static;
    }
    auto initialized_runtime = EnsureFileInitialized(runtime_evidence_file_);
    if (initialized_runtime.IsError()) {
        return initialized_runtime;
    }
    auto initialized_candidates = EnsureFileInitialized(candidates_file_);
    if (initialized_candidates.IsError()) {
        return initialized_candidates;
    }
    return EnsureFileInitialized(decision_logs_file_);
}

Result<BatchWriteSummary> JsonRedundancyRepositoryAdapter::UpsertEntities(const std::vector<Domain::System::Redundancy::CodeEntity>& entities) {
    std::scoped_lock lock(mutex_);
    auto initialized = EnsureStoreInitialized();
    if (initialized.IsError()) {
        return Result<BatchWriteSummary>::Failure(initialized.GetError());
    }
    auto existing_result = LoadRecords<Domain::System::Redundancy::CodeEntity>(entities_file_, ParseCodeEntity);
    if (existing_result.IsError()) {
        return Result<BatchWriteSummary>::Failure(existing_result.GetError());
    }

    std::vector<Domain::System::Redundancy::CodeEntity> valid_records;
    BatchWriteSummary summary;
    summary.received = entities.size();
    for (const auto& entity : entities) {
        if (!entity.Validate()) {
            ++summary.rejected;
            continue;
        }
        valid_records.push_back(entity);
    }

    auto existing = existing_result.Value();
    summary.upserted = UpsertById(existing, valid_records, [](const auto& item) { return item.entity_id; });
    auto save_result = SaveRecords(
        entities_file_,
        existing,
        [](const Domain::System::Redundancy::CodeEntity& item) { return ToJson(item); });
    if (save_result.IsError()) {
        return Result<BatchWriteSummary>::Failure(save_result.GetError());
    }
    return Result<BatchWriteSummary>::Success(summary);
}

Result<BatchWriteSummary> JsonRedundancyRepositoryAdapter::AppendStaticEvidence(
    const std::vector<Domain::System::Redundancy::StaticEvidenceRecord>& records) {
    std::scoped_lock lock(mutex_);
    auto initialized = EnsureStoreInitialized();
    if (initialized.IsError()) {
        return Result<BatchWriteSummary>::Failure(initialized.GetError());
    }
    auto existing_result = LoadRecords<Domain::System::Redundancy::StaticEvidenceRecord>(static_evidence_file_, ParseStaticEvidence);
    if (existing_result.IsError()) {
        return Result<BatchWriteSummary>::Failure(existing_result.GetError());
    }

    std::vector<Domain::System::Redundancy::StaticEvidenceRecord> valid_records;
    BatchWriteSummary summary;
    summary.received = records.size();
    for (const auto& record : records) {
        if (!record.Validate()) {
            ++summary.rejected;
            continue;
        }
        valid_records.push_back(record);
    }

    auto existing = existing_result.Value();
    summary.upserted = UpsertById(existing, valid_records, [](const auto& item) { return item.evidence_id; });
    auto save_result = SaveRecords(
        static_evidence_file_,
        existing,
        [](const Domain::System::Redundancy::StaticEvidenceRecord& item) { return ToJson(item); });
    if (save_result.IsError()) {
        return Result<BatchWriteSummary>::Failure(save_result.GetError());
    }
    return Result<BatchWriteSummary>::Success(summary);
}

Result<BatchWriteSummary> JsonRedundancyRepositoryAdapter::AppendRuntimeEvidence(
    const std::vector<Domain::System::Redundancy::RuntimeEvidenceRecord>& records) {
    std::scoped_lock lock(mutex_);
    auto initialized = EnsureStoreInitialized();
    if (initialized.IsError()) {
        return Result<BatchWriteSummary>::Failure(initialized.GetError());
    }
    auto existing_result = LoadRecords<Domain::System::Redundancy::RuntimeEvidenceRecord>(runtime_evidence_file_, ParseRuntimeEvidence);
    if (existing_result.IsError()) {
        return Result<BatchWriteSummary>::Failure(existing_result.GetError());
    }

    std::vector<Domain::System::Redundancy::RuntimeEvidenceRecord> valid_records;
    BatchWriteSummary summary;
    summary.received = records.size();
    for (const auto& record : records) {
        if (!record.Validate()) {
            ++summary.rejected;
            continue;
        }
        valid_records.push_back(record);
    }

    auto existing = existing_result.Value();
    summary.upserted = UpsertById(existing, valid_records, [](const auto& item) { return item.evidence_id; });
    auto save_result = SaveRecords(
        runtime_evidence_file_,
        existing,
        [](const Domain::System::Redundancy::RuntimeEvidenceRecord& item) { return ToJson(item); });
    if (save_result.IsError()) {
        return Result<BatchWriteSummary>::Failure(save_result.GetError());
    }
    return Result<BatchWriteSummary>::Success(summary);
}

Result<BatchWriteSummary> JsonRedundancyRepositoryAdapter::UpsertCandidates(
    const std::vector<Domain::System::Redundancy::CandidateRecord>& records) {
    std::scoped_lock lock(mutex_);
    auto initialized = EnsureStoreInitialized();
    if (initialized.IsError()) {
        return Result<BatchWriteSummary>::Failure(initialized.GetError());
    }
    auto existing_result = LoadRecords<Domain::System::Redundancy::CandidateRecord>(candidates_file_, ParseCandidateRecord);
    if (existing_result.IsError()) {
        return Result<BatchWriteSummary>::Failure(existing_result.GetError());
    }

    std::vector<Domain::System::Redundancy::CandidateRecord> valid_records;
    BatchWriteSummary summary;
    summary.received = records.size();
    for (const auto& record : records) {
        if (!record.Validate()) {
            ++summary.rejected;
            continue;
        }
        valid_records.push_back(record);
    }

    auto existing = existing_result.Value();
    summary.upserted = UpsertById(existing, valid_records, [](const auto& item) { return item.candidate_id; });
    auto save_result = SaveRecords(
        candidates_file_,
        existing,
        [](const Domain::System::Redundancy::CandidateRecord& item) { return ToJson(item); });
    if (save_result.IsError()) {
        return Result<BatchWriteSummary>::Failure(save_result.GetError());
    }
    return Result<BatchWriteSummary>::Success(summary);
}

Result<BatchWriteSummary> JsonRedundancyRepositoryAdapter::AppendDecisionLog(const Domain::System::Redundancy::DecisionLogRecord& record) {
    std::scoped_lock lock(mutex_);
    auto initialized = EnsureStoreInitialized();
    if (initialized.IsError()) {
        return Result<BatchWriteSummary>::Failure(initialized.GetError());
    }
    if (!record.Validate()) {
        return Result<BatchWriteSummary>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "DecisionLogRecord validation failed", "JsonRedundancyRepositoryAdapter"));
    }

    auto existing_result = LoadRecords<Domain::System::Redundancy::DecisionLogRecord>(decision_logs_file_, ParseDecisionLogRecord);
    if (existing_result.IsError()) {
        return Result<BatchWriteSummary>::Failure(existing_result.GetError());
    }

    BatchWriteSummary summary;
    summary.received = 1;
    auto existing = existing_result.Value();
    auto duplicate_it = std::find_if(existing.begin(), existing.end(), [&record](const auto& current) {
        return current.decision_id == record.decision_id;
    });
    if (duplicate_it != existing.end()) {
        if (IsEquivalentDecisionLogRecord(*duplicate_it, record)) {
            return Result<BatchWriteSummary>::Success(summary);
        }
        return Result<BatchWriteSummary>::Failure(
            Error(ErrorCode::DUPLICATE_DECISION_ID, "duplicate_decision_id", "JsonRedundancyRepositoryAdapter"));
    }

    existing.push_back(record);
    summary.upserted = 1;
    auto save_result = SaveRecords(
        decision_logs_file_,
        existing,
        [](const Domain::System::Redundancy::DecisionLogRecord& item) { return ToJson(item); });
    if (save_result.IsError()) {
        return Result<BatchWriteSummary>::Failure(save_result.GetError());
    }
    return Result<BatchWriteSummary>::Success(summary);
}

Result<std::vector<Domain::System::Redundancy::CodeEntity>> JsonRedundancyRepositoryAdapter::ListEntities(const std::string& module) const {
    std::scoped_lock lock(mutex_);
    auto initialized = EnsureStoreInitialized();
    if (initialized.IsError()) {
        return Result<std::vector<Domain::System::Redundancy::CodeEntity>>::Failure(initialized.GetError());
    }
    auto records_result = LoadRecords<Domain::System::Redundancy::CodeEntity>(entities_file_, ParseCodeEntity);
    if (records_result.IsError()) {
        return Result<std::vector<Domain::System::Redundancy::CodeEntity>>::Failure(records_result.GetError());
    }
    if (module.empty()) {
        return records_result;
    }

    std::vector<Domain::System::Redundancy::CodeEntity> filtered;
    for (const auto& item : records_result.Value()) {
        if (item.module == module) {
            filtered.push_back(item);
        }
    }
    return Result<std::vector<Domain::System::Redundancy::CodeEntity>>::Success(std::move(filtered));
}

Result<std::vector<Domain::System::Redundancy::StaticEvidenceRecord>> JsonRedundancyRepositoryAdapter::ListStaticEvidence(
    const std::string& module) const {
    std::scoped_lock lock(mutex_);
    auto initialized = EnsureStoreInitialized();
    if (initialized.IsError()) {
        return Result<std::vector<Domain::System::Redundancy::StaticEvidenceRecord>>::Failure(initialized.GetError());
    }
    auto records_result = LoadRecords<Domain::System::Redundancy::StaticEvidenceRecord>(static_evidence_file_, ParseStaticEvidence);
    if (records_result.IsError()) {
        return records_result;
    }
    if (module.empty()) {
        return records_result;
    }

    auto entities_result = LoadRecords<Domain::System::Redundancy::CodeEntity>(entities_file_, ParseCodeEntity);
    if (entities_result.IsError()) {
        return Result<std::vector<Domain::System::Redundancy::StaticEvidenceRecord>>::Failure(entities_result.GetError());
    }
    const auto module_map = BuildEntityModuleMap(entities_result.Value());

    std::vector<Domain::System::Redundancy::StaticEvidenceRecord> filtered;
    for (const auto& item : records_result.Value()) {
        auto it = module_map.find(item.entity_id);
        if (it != module_map.end() && it->second == module) {
            filtered.push_back(item);
        }
    }
    return Result<std::vector<Domain::System::Redundancy::StaticEvidenceRecord>>::Success(std::move(filtered));
}

Result<std::vector<Domain::System::Redundancy::RuntimeEvidenceRecord>> JsonRedundancyRepositoryAdapter::ListRuntimeEvidence(
    const std::string& module,
    const std::string& window_start,
    const std::string& window_end) const {
    std::scoped_lock lock(mutex_);
    auto initialized = EnsureStoreInitialized();
    if (initialized.IsError()) {
        return Result<std::vector<Domain::System::Redundancy::RuntimeEvidenceRecord>>::Failure(initialized.GetError());
    }
    auto records_result = LoadRecords<Domain::System::Redundancy::RuntimeEvidenceRecord>(runtime_evidence_file_, ParseRuntimeEvidence);
    if (records_result.IsError()) {
        return records_result;
    }

    std::unordered_map<std::string, std::string> module_map;
    if (!module.empty()) {
        auto entities_result = LoadRecords<Domain::System::Redundancy::CodeEntity>(entities_file_, ParseCodeEntity);
        if (entities_result.IsError()) {
            return Result<std::vector<Domain::System::Redundancy::RuntimeEvidenceRecord>>::Failure(entities_result.GetError());
        }
        module_map = BuildEntityModuleMap(entities_result.Value());
    }

    std::vector<Domain::System::Redundancy::RuntimeEvidenceRecord> filtered;
    for (const auto& item : records_result.Value()) {
        if (!module.empty()) {
            auto it = module_map.find(item.entity_id);
            if (it == module_map.end() || it->second != module) {
                continue;
            }
        }
        if (!WindowOverlaps(item.window_start, item.window_end, window_start, window_end)) {
            continue;
        }
        filtered.push_back(item);
    }
    return Result<std::vector<Domain::System::Redundancy::RuntimeEvidenceRecord>>::Success(std::move(filtered));
}

Result<std::vector<Domain::System::Redundancy::CandidateRecord>> JsonRedundancyRepositoryAdapter::ListCandidates(
    const CandidateQueryFilter& filter) const {
    std::scoped_lock lock(mutex_);
    auto initialized = EnsureStoreInitialized();
    if (initialized.IsError()) {
        return Result<std::vector<Domain::System::Redundancy::CandidateRecord>>::Failure(initialized.GetError());
    }
    auto records_result = LoadRecords<Domain::System::Redundancy::CandidateRecord>(candidates_file_, ParseCandidateRecord);
    if (records_result.IsError()) {
        return Result<std::vector<Domain::System::Redundancy::CandidateRecord>>::Failure(records_result.GetError());
    }

    std::unordered_map<std::string, std::string> module_map;
    if (!filter.module.empty()) {
        auto entities_result = LoadRecords<Domain::System::Redundancy::CodeEntity>(entities_file_, ParseCodeEntity);
        if (entities_result.IsError()) {
            return Result<std::vector<Domain::System::Redundancy::CandidateRecord>>::Failure(entities_result.GetError());
        }
        module_map = BuildEntityModuleMap(entities_result.Value());
    }

    std::optional<Domain::System::Redundancy::CandidatePriority> parsed_priority_filter;
    if (filter.priority.has_value()) {
        parsed_priority_filter = CandidatePriorityFromString(filter.priority.value());
        if (!parsed_priority_filter.has_value()) {
            return Result<std::vector<Domain::System::Redundancy::CandidateRecord>>::Failure(
                Error(
                    ErrorCode::INVALID_PARAMETER,
                    "failure_stage=list_candidates_filter;failure_code=INVALID_PARAMETER;message=invalid_priority:" +
                        filter.priority.value(),
                    "JsonRedundancyRepositoryAdapter"));
        }
    }

    std::vector<Domain::System::Redundancy::CandidateRecord> filtered;
    for (const auto& item : records_result.Value()) {
        if (!filter.module.empty()) {
            auto it = module_map.find(item.entity_id);
            if (it == module_map.end() || it->second != filter.module) {
                continue;
            }
        }
        if (filter.status.has_value() && item.status != filter.status.value()) {
            continue;
        }
        if (parsed_priority_filter.has_value() && item.priority != parsed_priority_filter.value()) {
            continue;
        }
        if (filter.min_redundancy_score.has_value() && item.redundancy_score < filter.min_redundancy_score.value()) {
            continue;
        }
        if (filter.max_deletion_risk_score.has_value() && item.deletion_risk_score > filter.max_deletion_risk_score.value()) {
            continue;
        }
        filtered.push_back(item);
    }

    if (filter.offset >= filtered.size()) {
        return Result<std::vector<Domain::System::Redundancy::CandidateRecord>>::Success({});
    }
    const auto end = std::min(filtered.size(), filter.offset + filter.limit);
    std::vector<Domain::System::Redundancy::CandidateRecord> paged(
        filtered.begin() + static_cast<std::ptrdiff_t>(filter.offset),
        filtered.begin() + static_cast<std::ptrdiff_t>(end));
    return Result<std::vector<Domain::System::Redundancy::CandidateRecord>>::Success(std::move(paged));
}

Result<Domain::System::Redundancy::CandidateRecord> JsonRedundancyRepositoryAdapter::GetCandidateById(
    const std::string& candidate_id) const {
    std::scoped_lock lock(mutex_);
    auto initialized = EnsureStoreInitialized();
    if (initialized.IsError()) {
        return Result<Domain::System::Redundancy::CandidateRecord>::Failure(initialized.GetError());
    }
    auto records_result = LoadRecords<Domain::System::Redundancy::CandidateRecord>(candidates_file_, ParseCandidateRecord);
    if (records_result.IsError()) {
        return Result<Domain::System::Redundancy::CandidateRecord>::Failure(records_result.GetError());
    }
    for (const auto& item : records_result.Value()) {
        if (item.candidate_id == candidate_id) {
            return Result<Domain::System::Redundancy::CandidateRecord>::Success(item);
        }
    }
    return Result<Domain::System::Redundancy::CandidateRecord>::Failure(
        Error(ErrorCode::NOT_FOUND, "Candidate not found: " + candidate_id, "JsonRedundancyRepositoryAdapter"));
}

Result<void> JsonRedundancyRepositoryAdapter::TransitionCandidateStatus(const CandidateTransitionRequest& request) {
    std::scoped_lock lock(mutex_);
    auto initialized = EnsureStoreInitialized();
    if (initialized.IsError()) {
        return initialized;
    }
    if (!request.Validate()) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Invalid transition request", "JsonRedundancyRepositoryAdapter"));
    }

    auto records_result = LoadRecords<Domain::System::Redundancy::CandidateRecord>(candidates_file_, ParseCandidateRecord);
    if (records_result.IsError()) {
        return Result<void>::Failure(records_result.GetError());
    }

    auto candidates = records_result.Value();
    bool found = false;
    for (auto& item : candidates) {
        if (item.candidate_id != request.candidate_id) {
            continue;
        }
        found = true;
        if (item.status != request.from_status) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "Transition from_status mismatch", "JsonRedundancyRepositoryAdapter"));
        }
        item.status = request.to_status;
        break;
    }

    if (!found) {
        return Result<void>::Failure(
            Error(ErrorCode::NOT_FOUND, "Candidate not found: " + request.candidate_id, "JsonRedundancyRepositoryAdapter"));
    }

    auto save_result = SaveRecords(
        candidates_file_,
        candidates,
        [](const Domain::System::Redundancy::CandidateRecord& item) { return ToJson(item); });
    if (save_result.IsError()) {
        return save_result;
    }
    return Result<void>::Success();
}

}  // namespace Siligen::Infrastructure::Adapters::Redundancy
