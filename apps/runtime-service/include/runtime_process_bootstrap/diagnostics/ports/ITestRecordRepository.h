#pragma once

#include "runtime_process_bootstrap/diagnostics/aggregates/TestRecord.h"
#include "runtime_process_bootstrap/diagnostics/value-objects/TestDataTypes.h"
#include "shared/types/Result.h"

#include <cstdint>
#include <vector>

namespace Siligen::Domain::Diagnostics::Ports {

using Shared::Types::Result;
using Siligen::Domain::Diagnostics::Aggregates::TestRecord;
using Siligen::Domain::Diagnostics::Aggregates::TestStatus;
using Siligen::Domain::Diagnostics::Aggregates::TestType;
using Siligen::Domain::Diagnostics::ValueObjects::CMPTestData;
using Siligen::Domain::Diagnostics::ValueObjects::DiagnosticResult;
using Siligen::Domain::Diagnostics::ValueObjects::HomingTestData;
using Siligen::Domain::Diagnostics::ValueObjects::InterpolationTestData;
using Siligen::Domain::Diagnostics::ValueObjects::JogTestData;
using Siligen::Domain::Diagnostics::ValueObjects::TriggerTestData;

class ITestRecordRepository {
   public:
    virtual ~ITestRecordRepository() = default;

    virtual Result<std::int64_t> saveTestRecord(const TestRecord& record) = 0;
    virtual Result<TestRecord> getTestRecordById(std::int64_t id) const = 0;
    virtual Result<void> deleteTestRecord(std::int64_t id) = 0;

    virtual Result<void> saveJogTestData(const JogTestData& data) = 0;
    virtual Result<JogTestData> getJogTestData(std::int64_t recordId) const = 0;

    virtual Result<void> saveHomingTestData(const HomingTestData& data) = 0;
    virtual Result<HomingTestData> getHomingTestData(std::int64_t recordId) const = 0;

    virtual Result<void> saveTriggerTestData(const TriggerTestData& data) = 0;
    virtual Result<TriggerTestData> getTriggerTestData(std::int64_t recordId) const = 0;

    virtual Result<void> saveCMPTestData(const CMPTestData& data) = 0;
    virtual Result<CMPTestData> getCMPTestData(std::int64_t recordId) const = 0;

    virtual Result<void> saveInterpolationTestData(const InterpolationTestData& data) = 0;
    virtual Result<InterpolationTestData> getInterpolationTestData(std::int64_t recordId) const = 0;

    virtual Result<void> saveDiagnosticResult(const DiagnosticResult& data) = 0;
    virtual Result<DiagnosticResult> getDiagnosticResult(std::int64_t recordId) const = 0;

    virtual std::vector<TestRecord> queryByTestType(TestType testType, int limit = 0) const = 0;
    virtual std::vector<TestRecord> queryByTimeRange(std::int64_t startTimestamp,
                                                     std::int64_t endTimestamp,
                                                     int limit = 0) const = 0;
    virtual std::vector<TestRecord> queryByOperator(const std::string& operatorName, int limit = 0) const = 0;
    virtual std::vector<TestRecord> queryByStatus(TestStatus status, int limit = 0) const = 0;
    virtual std::vector<TestRecord> getRecentRecords(int limit) const = 0;
    virtual std::int64_t getRecordCount() const = 0;

    virtual Result<int> purgeOldRecords(std::int64_t beforeTimestamp) = 0;
    virtual Result<void> optimizeDatabase() = 0;
};

}  // namespace Siligen::Domain::Diagnostics::Ports
