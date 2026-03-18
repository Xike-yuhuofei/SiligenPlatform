#include "domain/diagnostics/domain-services/ProcessResultService.h"

#include "domain/diagnostics/domain-services/ProcessResultSerialization.h"
#include "shared/types/Error.h"

#include <ctime>

namespace Siligen::Domain::Diagnostics::DomainServices {
namespace {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

Result<void> MakeValidationError(const std::string& message) {
    return Result<void>::Failure(Error(ErrorCode::ValidationFailed, message, "ProcessResultService"));
}

Result<std::int64_t> MakeRepositoryError(const std::string& message) {
    return Result<std::int64_t>::Failure(Error(ErrorCode::REPOSITORY_NOT_AVAILABLE, message, "ProcessResultService"));
}

}  // namespace

ProcessResultService::ProcessResultService(std::shared_ptr<ITestRecordRepository> repository) noexcept
    : repository_(std::move(repository)) {}

Result<void> ProcessResultService::ValidateBaseRecord(const TestRecord& record) noexcept {
    if (record.operatorName.empty()) {
        return MakeValidationError("Operator name is required");
    }
    if (record.durationMs < 0) {
        return MakeValidationError("Duration must be non-negative");
    }
    if (record.status != Aggregates::TestStatus::Success && !record.errorMessage.has_value()) {
        return MakeValidationError("Error message required for non-success status");
    }
    if (record.timestamp <= 0) {
        return MakeValidationError("Timestamp must be set");
    }
    return Result<void>::Success();
}

Result<TestRecord> ProcessResultService::BuildRecord(const TestRecord& base,
                                                     TestType type,
                                                     const std::string& parameters_json,
                                                     const std::string& result_json) noexcept {
    TestRecord record = base;
    record.testType = type;
    if (record.timestamp <= 0) {
        record.timestamp = static_cast<std::int64_t>(std::time(nullptr));
    }
    record.parametersJson = parameters_json;
    record.resultJson = result_json;

    auto validation = ValidateBaseRecord(record);
    if (validation.IsError()) {
        return Result<TestRecord>::Failure(validation.GetError());
    }

    return Result<TestRecord>::Success(record);
}

Result<TestRecord> ProcessResultService::BuildJogRecord(const TestRecord& base, const JogTestData& data) noexcept {
    if (!data.isValid()) {
        return Result<TestRecord>::Failure(Error(ErrorCode::ValidationFailed, "Invalid jog test data", "ProcessResultService"));
    }
    auto params = Serialization::SerializeJogParameters(data);
    if (params.IsError()) {
        return Result<TestRecord>::Failure(params.GetError());
    }
    auto results = Serialization::SerializeJogResult(data);
    if (results.IsError()) {
        return Result<TestRecord>::Failure(results.GetError());
    }
    return BuildRecord(base, TestType::Jog, params.Value(), results.Value());
}

Result<TestRecord> ProcessResultService::BuildHomingRecord(const TestRecord& base, const HomingTestData& data) noexcept {
    if (!data.isValid()) {
        return Result<TestRecord>::Failure(Error(ErrorCode::ValidationFailed, "Invalid homing test data", "ProcessResultService"));
    }
    auto params = Serialization::SerializeHomingParameters(data);
    if (params.IsError()) {
        return Result<TestRecord>::Failure(params.GetError());
    }
    auto results = Serialization::SerializeHomingResult(data);
    if (results.IsError()) {
        return Result<TestRecord>::Failure(results.GetError());
    }
    return BuildRecord(base, TestType::Homing, params.Value(), results.Value());
}

Result<TestRecord> ProcessResultService::BuildTriggerRecord(const TestRecord& base, const TriggerTestData& data) noexcept {
    if (!data.isValid()) {
        return Result<TestRecord>::Failure(Error(ErrorCode::ValidationFailed, "Invalid trigger test data", "ProcessResultService"));
    }
    auto params = Serialization::SerializeTriggerParameters(data);
    if (params.IsError()) {
        return Result<TestRecord>::Failure(params.GetError());
    }
    auto results = Serialization::SerializeTriggerResult(data);
    if (results.IsError()) {
        return Result<TestRecord>::Failure(results.GetError());
    }
    return BuildRecord(base, TestType::Trigger, params.Value(), results.Value());
}

Result<TestRecord> ProcessResultService::BuildCMPRecord(const TestRecord& base, const CMPTestData& data) noexcept {
    if (!data.isValid()) {
        return Result<TestRecord>::Failure(Error(ErrorCode::ValidationFailed, "Invalid CMP test data", "ProcessResultService"));
    }
    auto params = Serialization::SerializeCMPParameters(data);
    if (params.IsError()) {
        return Result<TestRecord>::Failure(params.GetError());
    }
    auto results = Serialization::SerializeCMPResult(data);
    if (results.IsError()) {
        return Result<TestRecord>::Failure(results.GetError());
    }
    return BuildRecord(base, TestType::CMP, params.Value(), results.Value());
}

Result<TestRecord> ProcessResultService::BuildInterpolationRecord(const TestRecord& base,
                                                                  const InterpolationTestData& data) noexcept {
    if (!data.isValid()) {
        return Result<TestRecord>::Failure(Error(ErrorCode::ValidationFailed, "Invalid interpolation test data",
                                                 "ProcessResultService"));
    }
    auto params = Serialization::SerializeInterpolationParameters(data);
    if (params.IsError()) {
        return Result<TestRecord>::Failure(params.GetError());
    }
    auto results = Serialization::SerializeInterpolationResult(data);
    if (results.IsError()) {
        return Result<TestRecord>::Failure(results.GetError());
    }
    return BuildRecord(base, TestType::Interpolation, params.Value(), results.Value());
}

Result<TestRecord> ProcessResultService::BuildDiagnosticRecord(const TestRecord& base,
                                                               const DiagnosticResult& data) noexcept {
    if (!data.isValid()) {
        return Result<TestRecord>::Failure(Error(ErrorCode::ValidationFailed, "Invalid diagnostic result data",
                                                 "ProcessResultService"));
    }
    auto results = Serialization::SerializeDiagnosticResult(data);
    if (results.IsError()) {
        return Result<TestRecord>::Failure(results.GetError());
    }
    return BuildRecord(base, TestType::Diagnostic, "{}", results.Value());
}

Result<std::int64_t> ProcessResultService::SaveJogResult(const TestRecord& base, const JogTestData& data) const noexcept {
    if (!repository_) {
        return MakeRepositoryError("Test record repository not available");
    }
    auto record_result = BuildJogRecord(base, data);
    if (record_result.IsError()) {
        return Result<std::int64_t>::Failure(record_result.GetError());
    }
    auto save_record = repository_->saveTestRecord(record_result.Value());
    if (save_record.IsError()) {
        return Result<std::int64_t>::Failure(save_record.GetError());
    }
    auto id = save_record.Value();
    auto persisted = data;
    persisted.recordId = id;
    auto save_data = repository_->saveJogTestData(persisted);
    if (save_data.IsError()) {
        return Result<std::int64_t>::Failure(save_data.GetError());
    }
    return Result<std::int64_t>::Success(id);
}

Result<std::int64_t> ProcessResultService::SaveHomingResult(const TestRecord& base, const HomingTestData& data) const noexcept {
    if (!repository_) {
        return MakeRepositoryError("Test record repository not available");
    }
    auto record_result = BuildHomingRecord(base, data);
    if (record_result.IsError()) {
        return Result<std::int64_t>::Failure(record_result.GetError());
    }
    auto save_record = repository_->saveTestRecord(record_result.Value());
    if (save_record.IsError()) {
        return Result<std::int64_t>::Failure(save_record.GetError());
    }
    auto id = save_record.Value();
    auto persisted = data;
    persisted.recordId = id;
    auto save_data = repository_->saveHomingTestData(persisted);
    if (save_data.IsError()) {
        return Result<std::int64_t>::Failure(save_data.GetError());
    }
    return Result<std::int64_t>::Success(id);
}

Result<std::int64_t> ProcessResultService::SaveTriggerResult(const TestRecord& base, const TriggerTestData& data) const noexcept {
    if (!repository_) {
        return MakeRepositoryError("Test record repository not available");
    }
    auto record_result = BuildTriggerRecord(base, data);
    if (record_result.IsError()) {
        return Result<std::int64_t>::Failure(record_result.GetError());
    }
    auto save_record = repository_->saveTestRecord(record_result.Value());
    if (save_record.IsError()) {
        return Result<std::int64_t>::Failure(save_record.GetError());
    }
    auto id = save_record.Value();
    auto persisted = data;
    persisted.recordId = id;
    auto save_data = repository_->saveTriggerTestData(persisted);
    if (save_data.IsError()) {
        return Result<std::int64_t>::Failure(save_data.GetError());
    }
    return Result<std::int64_t>::Success(id);
}

Result<std::int64_t> ProcessResultService::SaveCMPResult(const TestRecord& base, const CMPTestData& data) const noexcept {
    if (!repository_) {
        return MakeRepositoryError("Test record repository not available");
    }
    auto record_result = BuildCMPRecord(base, data);
    if (record_result.IsError()) {
        return Result<std::int64_t>::Failure(record_result.GetError());
    }
    auto save_record = repository_->saveTestRecord(record_result.Value());
    if (save_record.IsError()) {
        return Result<std::int64_t>::Failure(save_record.GetError());
    }
    auto id = save_record.Value();
    auto persisted = data;
    persisted.recordId = id;
    auto save_data = repository_->saveCMPTestData(persisted);
    if (save_data.IsError()) {
        return Result<std::int64_t>::Failure(save_data.GetError());
    }
    return Result<std::int64_t>::Success(id);
}

Result<std::int64_t> ProcessResultService::SaveInterpolationResult(const TestRecord& base,
                                                                   const InterpolationTestData& data) const noexcept {
    if (!repository_) {
        return MakeRepositoryError("Test record repository not available");
    }
    auto record_result = BuildInterpolationRecord(base, data);
    if (record_result.IsError()) {
        return Result<std::int64_t>::Failure(record_result.GetError());
    }
    auto save_record = repository_->saveTestRecord(record_result.Value());
    if (save_record.IsError()) {
        return Result<std::int64_t>::Failure(save_record.GetError());
    }
    auto id = save_record.Value();
    auto persisted = data;
    persisted.recordId = id;
    auto save_data = repository_->saveInterpolationTestData(persisted);
    if (save_data.IsError()) {
        return Result<std::int64_t>::Failure(save_data.GetError());
    }
    return Result<std::int64_t>::Success(id);
}

Result<std::int64_t> ProcessResultService::SaveDiagnosticResult(const TestRecord& base,
                                                                const DiagnosticResult& data) const noexcept {
    if (!repository_) {
        return MakeRepositoryError("Test record repository not available");
    }
    auto record_result = BuildDiagnosticRecord(base, data);
    if (record_result.IsError()) {
        return Result<std::int64_t>::Failure(record_result.GetError());
    }
    auto save_record = repository_->saveTestRecord(record_result.Value());
    if (save_record.IsError()) {
        return Result<std::int64_t>::Failure(save_record.GetError());
    }
    auto id = save_record.Value();
    auto persisted = data;
    persisted.recordId = id;
    auto save_data = repository_->saveDiagnosticResult(persisted);
    if (save_data.IsError()) {
        return Result<std::int64_t>::Failure(save_data.GetError());
    }
    return Result<std::int64_t>::Success(id);
}

}  // namespace Siligen::Domain::Diagnostics::DomainServices
