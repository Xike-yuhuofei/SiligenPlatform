#pragma once

#include "domain/diagnostics/aggregates/TestRecord.h"
#include "domain/diagnostics/ports/ITestRecordRepository.h"
#include "domain/diagnostics/value-objects/TestDataTypes.h"
#include "shared/types/Result.h"

#include <cstdint>
#include <memory>

namespace Siligen::Domain::Diagnostics::DomainServices {

using Siligen::Shared::Types::Result;
using Siligen::Domain::Diagnostics::Aggregates::TestRecord;
using Siligen::Domain::Diagnostics::Aggregates::TestType;
using Siligen::Domain::Diagnostics::Ports::ITestRecordRepository;
using Siligen::Domain::Diagnostics::ValueObjects::CMPTestData;
using Siligen::Domain::Diagnostics::ValueObjects::DiagnosticResult;
using Siligen::Domain::Diagnostics::ValueObjects::HomingTestData;
using Siligen::Domain::Diagnostics::ValueObjects::InterpolationTestData;
using Siligen::Domain::Diagnostics::ValueObjects::JogTestData;
using Siligen::Domain::Diagnostics::ValueObjects::TriggerTestData;

/**
 * @brief 工艺/测试结果的统一构建与持久化入口
 *
 * 结构化定义与校验在 Domain 内完成；持久化通过 ITestRecordRepository 端口执行。
 */
class ProcessResultService {
   public:
    explicit ProcessResultService(std::shared_ptr<ITestRecordRepository> repository) noexcept;

    Result<std::int64_t> SaveJogResult(const TestRecord& base, const JogTestData& data) const noexcept;
    Result<std::int64_t> SaveHomingResult(const TestRecord& base, const HomingTestData& data) const noexcept;
    Result<std::int64_t> SaveTriggerResult(const TestRecord& base, const TriggerTestData& data) const noexcept;
    Result<std::int64_t> SaveCMPResult(const TestRecord& base, const CMPTestData& data) const noexcept;
    Result<std::int64_t> SaveInterpolationResult(const TestRecord& base, const InterpolationTestData& data) const noexcept;
    Result<std::int64_t> SaveDiagnosticResult(const TestRecord& base, const DiagnosticResult& data) const noexcept;

    static Result<TestRecord> BuildJogRecord(const TestRecord& base, const JogTestData& data) noexcept;
    static Result<TestRecord> BuildHomingRecord(const TestRecord& base, const HomingTestData& data) noexcept;
    static Result<TestRecord> BuildTriggerRecord(const TestRecord& base, const TriggerTestData& data) noexcept;
    static Result<TestRecord> BuildCMPRecord(const TestRecord& base, const CMPTestData& data) noexcept;
    static Result<TestRecord> BuildInterpolationRecord(const TestRecord& base, const InterpolationTestData& data) noexcept;
    static Result<TestRecord> BuildDiagnosticRecord(const TestRecord& base, const DiagnosticResult& data) noexcept;

   private:
    std::shared_ptr<ITestRecordRepository> repository_;

    static Result<TestRecord> BuildRecord(const TestRecord& base,
                                          TestType type,
                                          const std::string& parameters_json,
                                          const std::string& result_json) noexcept;
    static Result<void> ValidateBaseRecord(const TestRecord& record) noexcept;
};

}  // namespace Siligen::Domain::Diagnostics::DomainServices
