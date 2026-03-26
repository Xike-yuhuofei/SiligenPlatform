#pragma once

#include "domain/diagnostics/aggregates/TestRecord.h"
#include "domain/diagnostics/value-objects/TestDataTypes.h"
#include "shared/types/Result.h"

#include <cstdint>
#include <optional>
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

/**
 * @brief 测试记录仓储接口
 *
 * 定义测试记录的持久化操作,支持CRUD和查询。
 *
 * 实现者:
 * - SQLiteTestRecordRepository (infrastructure/adapters/system/persistence/test-records/) - SQLite实现
 * - MockTestRecordRepository (tests/mocks/) - 内存Mock实现
 *
 * 消费者:
 * - 各测试用例类 (application/usecases/)
 * - 测试数据查询服务 (application/Services/)
 */
class ITestRecordRepository {
   public:
    virtual ~ITestRecordRepository() = default;

    // ============ 基础CRUD操作 ============

    /**
     * @brief 保存测试记录
     * @param record 测试记录对象
     * @return 生成的记录ID或错误信息
     */
    virtual Result<std::int64_t> saveTestRecord(const TestRecord& record) = 0;

    /**
     * @brief 根据ID获取测试记录
     * @param id 记录ID
     * @return 测试记录或错误信息
     */
    virtual Result<TestRecord> getTestRecordById(std::int64_t id) const = 0;

    /**
     * @brief 删除测试记录
     * @param id 记录ID
     * @return Success或错误信息
     */
    virtual Result<void> deleteTestRecord(std::int64_t id) = 0;


    // ============ 点动测试数据 ============

    /**
     * @brief 保存点动测试数据
     * @param data 点动测试数据
     * @return Success或错误信息
     */
    virtual Result<void> saveJogTestData(const JogTestData& data) = 0;

    /**
     * @brief 获取点动测试数据
     * @param recordId 关联的TestRecord ID
     * @return 点动测试数据或错误信息
     */
    virtual Result<JogTestData> getJogTestData(std::int64_t recordId) const = 0;


    // ============ 回零测试数据 ============

    /**
     * @brief 保存回零测试数据
     * @param data 回零测试数据
     * @return Success或错误信息
     */
    virtual Result<void> saveHomingTestData(const HomingTestData& data) = 0;

    /**
     * @brief 获取回零测试数据
     * @param recordId 关联的TestRecord ID
     * @return 回零测试数据或错误信息
     */
    virtual Result<HomingTestData> getHomingTestData(std::int64_t recordId) const = 0;


    // ============ 触发测试数据 ============

    /**
     * @brief 保存触发测试数据
     * @param data 触发测试数据
     * @return Success或错误信息
     */
    virtual Result<void> saveTriggerTestData(const TriggerTestData& data) = 0;

    /**
     * @brief 获取触发测试数据
     * @param recordId 关联的TestRecord ID
     * @return 触发测试数据或错误信息
     */
    virtual Result<TriggerTestData> getTriggerTestData(std::int64_t recordId) const = 0;


    // ============ CMP测试数据 ============

    /**
     * @brief 保存CMP测试数据
     * @param data CMP测试数据
     * @return Success或错误信息
     */
    virtual Result<void> saveCMPTestData(const CMPTestData& data) = 0;

    /**
     * @brief 获取CMP测试数据
     * @param recordId 关联的TestRecord ID
     * @return CMP测试数据或错误信息
     */
    virtual Result<CMPTestData> getCMPTestData(std::int64_t recordId) const = 0;


    // ============ 插补测试数据 ============

    /**
     * @brief 保存插补测试数据
     * @param data 插补测试数据
     * @return Success或错误信息
     */
    virtual Result<void> saveInterpolationTestData(const InterpolationTestData& data) = 0;

    /**
     * @brief 获取插补测试数据
     * @param recordId 关联的TestRecord ID
     * @return 插补测试数据或错误信息
     */
    virtual Result<InterpolationTestData> getInterpolationTestData(std::int64_t recordId) const = 0;


    // ============ 诊断结果数据 ============

    /**
     * @brief 保存诊断结果数据
     * @param data 诊断结果数据
     * @return Success或错误信息
     */
    virtual Result<void> saveDiagnosticResult(const DiagnosticResult& data) = 0;

    /**
     * @brief 获取诊断结果数据
     * @param recordId 关联的TestRecord ID
     * @return 诊断结果数据或错误信息
     */
    virtual Result<DiagnosticResult> getDiagnosticResult(std::int64_t recordId) const = 0;


    // ============ 查询操作 ============

    /**
     * @brief 按测试类型查询记录
     * @param testType 测试类型
     * @param limit 返回数量限制(0=不限制)
     * @return 测试记录列表
     */
    virtual std::vector<TestRecord> queryByTestType(TestType testType, int limit = 0) const = 0;

    /**
     * @brief 按时间范围查询记录
     * @param startTimestamp 开始时间戳(秒)
     * @param endTimestamp 结束时间戳(秒)
     * @param limit 返回数量限制(0=不限制)
     * @return 测试记录列表
     */
    virtual std::vector<TestRecord> queryByTimeRange(std::int64_t startTimestamp,
                                                     std::int64_t endTimestamp,
                                                     int limit = 0) const = 0;

    /**
     * @brief 按操作人员查询记录
     * @param operatorName 操作人员名称
     * @param limit 返回数量限制(0=不限制)
     * @return 测试记录列表
     */
    virtual std::vector<TestRecord> queryByOperator(const std::string& operatorName, int limit = 0) const = 0;

    /**
     * @brief 按测试状态查询记录
     * @param status 测试状态(Success/Failure/Error)
     * @param limit 返回数量限制(0=不限制)
     * @return 测试记录列表
     */
    virtual std::vector<TestRecord> queryByStatus(TestStatus status, int limit = 0) const = 0;

    /**
     * @brief 获取最近N条记录
     * @param limit 返回数量
     * @return 测试记录列表(按时间倒序)
     */
    virtual std::vector<TestRecord> getRecentRecords(int limit) const = 0;

    /**
     * @brief 获取记录总数
     * @return 记录总数
     */
    virtual std::int64_t getRecordCount() const = 0;


    // ============ 维护操作 ============

    /**
     * @brief 清除旧记录
     * @param beforeTimestamp 删除此时间戳之前的记录
     * @return 删除的记录数量或错误信息
     */
    virtual Result<int> purgeOldRecords(std::int64_t beforeTimestamp) = 0;

    /**
     * @brief 优化数据库(SQLite: VACUUM)
     * @return Success或错误信息
     */
    virtual Result<void> optimizeDatabase() = 0;
};

}  // namespace Siligen::Domain::Diagnostics::Ports


