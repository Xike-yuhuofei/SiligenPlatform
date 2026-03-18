#pragma once

#include "domain/diagnostics/ports/ITestRecordRepository.h"

#include <map>
#include <memory>
#include <vector>

namespace Siligen::Infrastructure::Persistence {

using Siligen::Domain::Diagnostics::Aggregates::TestRecord;
using Siligen::Domain::Diagnostics::Aggregates::TestStatus;
using Siligen::Domain::Diagnostics::Aggregates::TestType;
using Siligen::Domain::Diagnostics::ValueObjects::CMPTestData;
using Siligen::Domain::Diagnostics::ValueObjects::DiagnosticResult;
using Siligen::Domain::Diagnostics::ValueObjects::HomingTestData;
using Siligen::Domain::Diagnostics::ValueObjects::InterpolationTestData;
using Siligen::Domain::Diagnostics::ValueObjects::JogTestData;
using Siligen::Domain::Diagnostics::ValueObjects::TriggerTestData;
using Siligen::Domain::Diagnostics::Ports::ITestRecordRepository;
using Siligen::Shared::Types::Result;

/**
 * @brief 内存Mock测试记录仓储实现
 *
 * 用于开发阶段的临时实现，避免SQLite依赖。
 * 数据仅保存在内存中，程序重启后丢失。
 */
class MockTestRecordRepository : public ITestRecordRepository {
   public:
    /**
     * @brief 构造函数
     */
    MockTestRecordRepository();

    /**
     * @brief 析构函数
     */
    ~MockTestRecordRepository() override = default;

    // 禁止拷贝
    MockTestRecordRepository(const MockTestRecordRepository&) = delete;
    MockTestRecordRepository& operator=(const MockTestRecordRepository&) = delete;

    // ============ ITestRecordRepository接口实现 ============

    Result<std::int64_t> saveTestRecord(const TestRecord& record) override;
    Result<TestRecord> getTestRecordById(std::int64_t id) const override;
    Result<void> deleteTestRecord(std::int64_t id) override;

    Result<void> saveJogTestData(const JogTestData& data) override;
    Result<JogTestData> getJogTestData(std::int64_t recordId) const override;

    Result<void> saveHomingTestData(const HomingTestData& data) override;
    Result<HomingTestData> getHomingTestData(std::int64_t recordId) const override;

    Result<void> saveTriggerTestData(const TriggerTestData& data) override;
    Result<TriggerTestData> getTriggerTestData(std::int64_t recordId) const override;

    Result<void> saveCMPTestData(const CMPTestData& data) override;
    Result<CMPTestData> getCMPTestData(std::int64_t recordId) const override;

    Result<void> saveInterpolationTestData(const InterpolationTestData& data) override;
    Result<InterpolationTestData> getInterpolationTestData(std::int64_t recordId) const override;

    Result<void> saveDiagnosticResult(const DiagnosticResult& data) override;
    Result<DiagnosticResult> getDiagnosticResult(std::int64_t recordId) const override;

    std::vector<TestRecord> queryByTestType(TestType testType, int limit = 0) const override;
    std::vector<TestRecord> queryByTimeRange(std::int64_t startTimestamp,
                                             std::int64_t endTimestamp,
                                             int limit = 0) const override;
    std::vector<TestRecord> queryByOperator(const std::string& operatorName, int limit = 0) const override;
    std::vector<TestRecord> queryByStatus(TestStatus status, int limit = 0) const override;
    std::vector<TestRecord> getRecentRecords(int limit) const override;
    std::int64_t getRecordCount() const override;

    Result<int> purgeOldRecords(std::int64_t beforeTimestamp) override;
    Result<void> optimizeDatabase() override;

   private:
    std::int64_t m_nextId;
    std::vector<TestRecord> m_testRecords;
    std::map<std::int64_t, JogTestData> m_jogTestDataMap;
    std::map<std::int64_t, HomingTestData> m_homingTestDataMap;
    std::map<std::int64_t, TriggerTestData> m_triggerTestDataMap;
    std::map<std::int64_t, CMPTestData> m_cmpTestDataMap;
    std::map<std::int64_t, InterpolationTestData> m_interpolationTestDataMap;
    std::map<std::int64_t, DiagnosticResult> m_diagnosticResultMap;
};

}  // namespace Siligen::Infrastructure::Persistence



