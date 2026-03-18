#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace Siligen {
namespace Domain {
namespace Diagnostics {
namespace Aggregates {

/**
 * @brief 测试类型枚举
 */
enum class TestType {
    Jog,            // 点动测试
    Homing,         // 回零测试
    IO,             // I/O测试
    Trigger,        // 位置触发测试
    CMP,            // CMP补偿测试
    Interpolation,  // 插补算法测试
    Diagnostic      // 系统诊断
};

/**
 * @brief 测试状态枚举
 */
enum class TestStatus {
    Success,  // 测试成功完成
    Failure,  // 测试失败(业务逻辑层面,如回零超时)
    Error     // 测试错误(系统层面,如硬件连接断开)
};

/**
 * @brief 测试状态枚举 - 运行时状态
 */
enum class TestState {
    Idle,       // 空闲,可启动新测试
    Running,    // 测试运行中
    Paused,     // 测试暂停
    Completed,  // 测试完成
    Error       // 测试错误
};

/**
 * @brief 测试记录基础实体
 *
 * 所有测试记录的基类,提供通用字段。
 * 具体测试数据通过 parametersJson/resultJson 序列化存储,
 * 持久化由 ITestRecordRepository 端口实现。
 */
class TestRecord {
   public:
    // 核心标识
    std::int64_t id;           // 数据库主键(自增)
    std::int64_t timestamp;    // Unix时间戳(秒)
    TestType testType;         // 测试类型枚举
    std::string operatorName;  // 操作人员名称

    // 测试状态
    TestStatus status;                        // success/failure/error
    std::int32_t durationMs;                  // 测试耗时(毫秒)
    std::optional<std::string> errorMessage;  // 错误信息(仅失败时)

    // 元数据
    std::string parametersJson;  // 测试参数(JSON序列化)
    std::string resultJson;      // 测试结果(JSON序列化)

    /**
     * @brief 构造函数
     */
    TestRecord()
        : id(0),
          timestamp(0),
          testType(TestType::Jog),
          operatorName(""),
          status(TestStatus::Success),
          durationMs(0),
          parametersJson("{}"),
          resultJson("{}") {}

    /**
     * @brief 验证记录有效性
     * @return true if valid, false otherwise
     */
    bool isValid() const {
        if (timestamp <= 0) return false;
        if (durationMs < 0) return false;
        if (operatorName.empty()) return false;
        if (status != TestStatus::Success && !errorMessage.has_value()) {
            return false;
        }
        return true;
    }

    /**
     * @brief 获取测试类型字符串表示
     */
    static std::string testTypeToString(TestType type) {
        switch (type) {
            case TestType::Jog:
                return "Jog";
            case TestType::Homing:
                return "Homing";
            case TestType::IO:
                return "IO";
            case TestType::Trigger:
                return "Trigger";
            case TestType::CMP:
                return "CMP";
            case TestType::Interpolation:
                return "Interpolation";
            case TestType::Diagnostic:
                return "Diagnostic";
            default:
                return "Unknown";
        }
    }

    /**
     * @brief 从字符串解析测试类型
     */
    static TestType stringToTestType(const std::string& str) {
        if (str == "Jog") return TestType::Jog;
        if (str == "Homing") return TestType::Homing;
        if (str == "IO") return TestType::IO;
        if (str == "Trigger") return TestType::Trigger;
        if (str == "CMP") return TestType::CMP;
        if (str == "Interpolation") return TestType::Interpolation;
        if (str == "Diagnostic") return TestType::Diagnostic;
        return TestType::Jog;  // 默认
    }
};

}  // namespace Aggregates
}  // namespace Diagnostics
}  // namespace Domain
}  // namespace Siligen
