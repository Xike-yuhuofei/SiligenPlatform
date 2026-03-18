#include "MockTestRecordRepository.h"

#include "shared/types/Error.h"

#include <algorithm>

namespace Siligen::Infrastructure::Persistence {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;

MockTestRecordRepository::MockTestRecordRepository() : m_nextId(1) {
    // 初始化空的数据容器
}

Result<std::int64_t> MockTestRecordRepository::saveTestRecord(const TestRecord& record) {
    TestRecord newRecord = record;
    newRecord.id = m_nextId++;

    m_testRecords.push_back(newRecord);
    return Result<std::int64_t>(newRecord.id);
}

Result<TestRecord> MockTestRecordRepository::getTestRecordById(std::int64_t id) const {
    for (const auto& record : m_testRecords) {
        if (record.id == id) {
            return Result<TestRecord>(record);
        }
    }

    return Result<TestRecord>::Failure(Error(
        ErrorCode::AXIS_NOT_FOUND, "Record not found with ID: " + std::to_string(id), "MockTestRecordRepository"));
}

Result<void> MockTestRecordRepository::deleteTestRecord(std::int64_t id) {
    auto it = std::find_if(
        m_testRecords.begin(), m_testRecords.end(), [id](const TestRecord& record) { return record.id == id; });

    if (it != m_testRecords.end()) {
        m_testRecords.erase(it);

        // 清理相关的测试数据
        m_jogTestDataMap.erase(id);
        m_homingTestDataMap.erase(id);
        m_triggerTestDataMap.erase(id);
        m_cmpTestDataMap.erase(id);
        m_interpolationTestDataMap.erase(id);
        m_diagnosticResultMap.erase(id);

        return Result<void>();
    }

    return Result<void>::Failure(Error(
        ErrorCode::AXIS_NOT_FOUND, "Record not found with ID: " + std::to_string(id), "MockTestRecordRepository"));
}

Result<void> MockTestRecordRepository::saveJogTestData(const JogTestData& data) {
    m_jogTestDataMap[data.recordId] = data;
    return Result<void>();
}

Result<JogTestData> MockTestRecordRepository::getJogTestData(std::int64_t recordId) const {
    auto it = m_jogTestDataMap.find(recordId);
    if (it != m_jogTestDataMap.end()) {
        return Result<JogTestData>(it->second);
    }

    return Result<JogTestData>::Failure(Error(ErrorCode::AXIS_NOT_FOUND,
                                              "Jog test data not found for record ID: " + std::to_string(recordId),
                                              "MockTestRecordRepository"));
}

Result<void> MockTestRecordRepository::saveHomingTestData(const HomingTestData& data) {
    m_homingTestDataMap[data.recordId] = data;
    return Result<void>();
}

Result<HomingTestData> MockTestRecordRepository::getHomingTestData(std::int64_t recordId) const {
    auto it = m_homingTestDataMap.find(recordId);
    if (it != m_homingTestDataMap.end()) {
        return Result<HomingTestData>(it->second);
    }

    return Result<HomingTestData>::Failure(
        Error(ErrorCode::AXIS_NOT_FOUND,
              "Homing test data not found for record ID: " + std::to_string(recordId),
              "MockTestRecordRepository"));
}

Result<void> MockTestRecordRepository::saveTriggerTestData(const TriggerTestData& data) {
    m_triggerTestDataMap[data.recordId] = data;
    return Result<void>();
}

Result<TriggerTestData> MockTestRecordRepository::getTriggerTestData(std::int64_t recordId) const {
    auto it = m_triggerTestDataMap.find(recordId);
    if (it != m_triggerTestDataMap.end()) {
        return Result<TriggerTestData>(it->second);
    }

    return Result<TriggerTestData>::Failure(
        Error(ErrorCode::AXIS_NOT_FOUND,
              "Trigger test data not found for record ID: " + std::to_string(recordId),
              "MockTestRecordRepository"));
}

Result<void> MockTestRecordRepository::saveCMPTestData(const CMPTestData& data) {
    m_cmpTestDataMap[data.recordId] = data;
    return Result<void>();
}

Result<CMPTestData> MockTestRecordRepository::getCMPTestData(std::int64_t recordId) const {
    auto it = m_cmpTestDataMap.find(recordId);
    if (it != m_cmpTestDataMap.end()) {
        return Result<CMPTestData>(it->second);
    }

    return Result<CMPTestData>::Failure(
        Error(ErrorCode::AXIS_NOT_FOUND,
              "CMP test data not found for record ID: " + std::to_string(recordId),
              "MockTestRecordRepository"));
}

Result<void> MockTestRecordRepository::saveInterpolationTestData(const InterpolationTestData& data) {
    m_interpolationTestDataMap[data.recordId] = data;
    return Result<void>();
}

Result<InterpolationTestData> MockTestRecordRepository::getInterpolationTestData(std::int64_t recordId) const {
    auto it = m_interpolationTestDataMap.find(recordId);
    if (it != m_interpolationTestDataMap.end()) {
        return Result<InterpolationTestData>(it->second);
    }

    return Result<InterpolationTestData>::Failure(
        Error(ErrorCode::AXIS_NOT_FOUND,
              "Interpolation test data not found for record ID: " + std::to_string(recordId),
              "MockTestRecordRepository"));
}

Result<void> MockTestRecordRepository::saveDiagnosticResult(const DiagnosticResult& data) {
    m_diagnosticResultMap[data.recordId] = data;
    return Result<void>();
}

Result<DiagnosticResult> MockTestRecordRepository::getDiagnosticResult(std::int64_t recordId) const {
    auto it = m_diagnosticResultMap.find(recordId);
    if (it != m_diagnosticResultMap.end()) {
        return Result<DiagnosticResult>(it->second);
    }

    return Result<DiagnosticResult>::Failure(
        Error(ErrorCode::AXIS_NOT_FOUND,
              "Diagnostic result not found for record ID: " + std::to_string(recordId),
              "MockTestRecordRepository"));
}

std::vector<TestRecord> MockTestRecordRepository::queryByTestType(TestType testType, int limit) const {
    std::vector<TestRecord> result;

    for (const auto& record : m_testRecords) {
        if (record.testType == testType) {
            result.push_back(record);
            if (limit > 0 && result.size() >= limit) {
                break;
            }
        }
    }

    return result;
}

std::vector<TestRecord> MockTestRecordRepository::queryByTimeRange(std::int64_t startTimestamp,
                                                                   std::int64_t endTimestamp,
                                                                   int limit) const {
    std::vector<TestRecord> result;

    for (const auto& record : m_testRecords) {
        if (record.timestamp >= startTimestamp && record.timestamp <= endTimestamp) {
            result.push_back(record);
            if (limit > 0 && result.size() >= limit) {
                break;
            }
        }
    }

    return result;
}

std::vector<TestRecord> MockTestRecordRepository::queryByOperator(const std::string& operatorName, int limit) const {
    std::vector<TestRecord> result;

    for (const auto& record : m_testRecords) {
        if (record.operatorName == operatorName) {
            result.push_back(record);
            if (limit > 0 && result.size() >= limit) {
                break;
            }
        }
    }

    return result;
}

std::vector<TestRecord> MockTestRecordRepository::queryByStatus(TestStatus status, int limit) const {
    std::vector<TestRecord> result;

    for (const auto& record : m_testRecords) {
        if (record.status == status) {
            result.push_back(record);
            if (limit > 0 && result.size() >= limit) {
                break;
            }
        }
    }

    return result;
}

std::vector<TestRecord> MockTestRecordRepository::getRecentRecords(int limit) const {
    std::vector<TestRecord> result = m_testRecords;

    // 按时间戳倒序排序
    std::sort(result.begin(), result.end(), [](const TestRecord& a, const TestRecord& b) {
        return a.timestamp > b.timestamp;
    });

    // 应用限制
    if (limit > 0 && result.size() > limit) {
        result.resize(limit);
    }

    return result;
}

std::int64_t MockTestRecordRepository::getRecordCount() const {
    return m_testRecords.size();
}

Result<int> MockTestRecordRepository::purgeOldRecords(std::int64_t beforeTimestamp) {
    int removedCount = 0;

    auto it = m_testRecords.begin();
    while (it != m_testRecords.end()) {
        if (it->timestamp < beforeTimestamp) {
            // 清理相关的测试数据
            std::int64_t id = it->id;
            m_jogTestDataMap.erase(id);
            m_homingTestDataMap.erase(id);
            m_triggerTestDataMap.erase(id);
            m_cmpTestDataMap.erase(id);
            m_interpolationTestDataMap.erase(id);
            m_diagnosticResultMap.erase(id);

            it = m_testRecords.erase(it);
            removedCount++;
        } else {
            ++it;
        }
    }

    return Result<int>(removedCount);
}

Result<void> MockTestRecordRepository::optimizeDatabase() {
    // Mock实现不需要优化
    return Result<void>();
}

}  // namespace Siligen::Infrastructure::Persistence

