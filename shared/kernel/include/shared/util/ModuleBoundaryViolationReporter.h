// ModuleBoundaryViolationReporter.h
// 版本: 1.0.0
// 描述: 模块边界违规检测和报告机制
//
// 六边形架构 - 运行时违规检测
// Task: T057 - Phase 4 建立清晰的模块边界

#pragma once

#include "ModuleDependencyCheck.h"

#include <chrono>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace Siligen {
namespace Shared {
namespace Util {

/// @brief 违规严重级别
enum class ViolationSeverity : int {
    INFO = 0,     // 信息级别(建议优化)
    WARNING = 1,  // 警告级别(应该修复)
    ERROR = 2,    // 错误级别(必须修复)
    CRITICAL = 3  // 严重级别(架构破坏)
};

/// @brief 违规类型
enum class ViolationType : int {
    INVALID_DEPENDENCY = 0,     // 非法依赖方向
    CIRCULAR_DEPENDENCY = 1,    // 循环依赖
    MISSING_MODULE_TAG = 2,     // 缺少模块标签
    LAYER_SKIP = 3,             // 跨层依赖
    DIRECT_IMPLEMENTATION = 4,  // 直接依赖实现而非接口
    UNKNOWN = 99
};

/// @brief 违规记录
struct ViolationRecord {
    ViolationType type;                                 // 违规类型
    ViolationSeverity severity;                         // 严重级别
    std::string source_module;                          // 源模块名称
    ModuleLayer source_layer;                           // 源模块层级
    std::string target_module;                          // 目标模块名称
    ModuleLayer target_layer;                           // 目标模块层级
    std::string description;                            // 违规描述
    std::string recommendation;                         // 修复建议
    std::string file_location;                          // 文件位置
    int line_number;                                    // 行号
    std::chrono::system_clock::time_point detected_at;  // 检测时间

    ViolationRecord()
        : type(ViolationType::UNKNOWN),
          severity(ViolationSeverity::INFO),
          source_module(""),
          source_layer(ModuleLayer::SHARED),
          target_module(""),
          target_layer(ModuleLayer::SHARED),
          description(""),
          recommendation(""),
          file_location(""),
          line_number(0),
          detected_at(std::chrono::system_clock::now()) {}
};

/// @brief 违规严重级别转字符串
inline const char* ViolationSeverityToString(ViolationSeverity severity) {
    switch (severity) {
        case ViolationSeverity::INFO:
            return "INFO";
        case ViolationSeverity::WARNING:
            return "WARNING";
        case ViolationSeverity::ERROR:
            return "ERROR";
        case ViolationSeverity::CRITICAL:
            return "CRITICAL";
        default:
            return "UNKNOWN";
    }
}

/// @brief 违规类型转字符串
inline const char* ViolationTypeToString(ViolationType type) {
    switch (type) {
        case ViolationType::INVALID_DEPENDENCY:
            return "Invalid Dependency";
        case ViolationType::CIRCULAR_DEPENDENCY:
            return "Circular Dependency";
        case ViolationType::MISSING_MODULE_TAG:
            return "Missing Module Tag";
        case ViolationType::LAYER_SKIP:
            return "Layer Skip";
        case ViolationType::DIRECT_IMPLEMENTATION:
            return "Direct Implementation Dependency";
        default:
            return "Unknown Violation";
    }
}

/// @brief 模块边界违规报告器
/// @details 收集、分析和报告模块边界违规情况
///
/// 功能:
/// - 运行时依赖违规检测
/// - 违规记录收集和存储
/// - 违规报告生成(文本、JSON、HTML)
/// - 违规趋势分析
/// - 违规修复建议
///
/// 使用场景:
/// - 持续集成流水线中的自动检测
/// - 开发环境中的实时违规提示
/// - 定期架构健康度报告
///
/// 使用示例:
/// @code
/// ModuleBoundaryViolationReporter reporter;
///
/// // 检测依赖违规
/// reporter.CheckDependency<MotionService, IHardwareController>(
///     "src/domain/services/MotionService.h", 42);
///
/// // 生成报告
/// std::string report = reporter.GenerateTextReport();
/// std::cout << report << std::endl;
///
/// // 获取统计信息
/// auto stats = reporter.GetStatistics();
/// std::cout << "总违规数: " << stats.total_violations << std::endl;
/// @endcode
class ModuleBoundaryViolationReporter {
   public:
    /// @brief 构造函数
    ModuleBoundaryViolationReporter() = default;

    ~ModuleBoundaryViolationReporter() = default;

    // 禁止拷贝和移动
    ModuleBoundaryViolationReporter(const ModuleBoundaryViolationReporter&) = delete;
    ModuleBoundaryViolationReporter& operator=(const ModuleBoundaryViolationReporter&) = delete;
    ModuleBoundaryViolationReporter(ModuleBoundaryViolationReporter&&) = delete;
    ModuleBoundaryViolationReporter& operator=(ModuleBoundaryViolationReporter&&) = delete;

    // ============================================================
    // 违规检测方法
    // ============================================================

    /// @brief 检查依赖是否合法(模板版本)
    /// @tparam Dependent 依赖者类型
    /// @tparam Dependency 被依赖者类型
    /// @param file_location 文件位置
    /// @param line_number 行号
    /// @return 是否合法
    template <typename Dependent, typename Dependency>
    bool CheckDependency(const std::string& file_location, int line_number) {
        constexpr ModuleLayer dependent_layer = GetModuleLayer<Dependent>::value;
        constexpr ModuleLayer dependency_layer = GetModuleLayer<Dependency>::value;

        bool is_valid = IsValidDependency(dependent_layer, dependency_layer);

        if (!is_valid) {
            ViolationRecord record;
            record.type = ViolationType::INVALID_DEPENDENCY;
            record.severity = ViolationSeverity::ERROR;
            record.source_module = typeid(Dependent).name();
            record.source_layer = dependent_layer;
            record.target_module = typeid(Dependency).name();
            record.target_layer = dependency_layer;
            record.file_location = file_location;
            record.line_number = line_number;
            record.description = GenerateViolationDescription(dependent_layer, dependency_layer);
            record.recommendation = GenerateRecommendation(dependent_layer, dependency_layer);

            violations_.push_back(record);
        }

        return is_valid;
    }

    /// @brief 检查依赖是否合法(运行时版本)
    /// @param dependent_layer 依赖者层级
    /// @param dependency_layer 被依赖者层级
    /// @param source_name 源模块名称
    /// @param target_name 目标模块名称
    /// @param file_location 文件位置
    /// @param line_number 行号
    /// @return 是否合法
    bool CheckDependencyRuntime(ModuleLayer dependent_layer,
                                ModuleLayer dependency_layer,
                                const std::string& source_name,
                                const std::string& target_name,
                                const std::string& file_location,
                                int line_number) {
        bool is_valid = IsValidDependency(dependent_layer, dependency_layer);

        if (!is_valid) {
            ViolationRecord record;
            record.type = ViolationType::INVALID_DEPENDENCY;
            record.severity = ViolationSeverity::ERROR;
            record.source_module = source_name;
            record.source_layer = dependent_layer;
            record.target_module = target_name;
            record.target_layer = dependency_layer;
            record.file_location = file_location;
            record.line_number = line_number;
            record.description = GenerateViolationDescription(dependent_layer, dependency_layer);
            record.recommendation = GenerateRecommendation(dependent_layer, dependency_layer);

            violations_.push_back(record);
        }

        return is_valid;
    }

    /// @brief 添加自定义违规记录
    /// @param record 违规记录
    void AddViolation(const ViolationRecord& record) {
        violations_.push_back(record);
    }

    // ============================================================
    // 报告生成方法
    // ============================================================

    /// @brief 生成文本格式报告
    /// @return 报告内容
    std::string GenerateTextReport() const {
        std::ostringstream oss;

        oss << "========================================\n";
        oss << "模块边界违规检测报告\n";
        oss << "========================================\n\n";

        auto stats = GetStatistics();

        oss << "总违规数: " << stats.total_violations << "\n";
        oss << "  - CRITICAL: " << stats.critical_count << "\n";
        oss << "  - ERROR: " << stats.error_count << "\n";
        oss << "  - WARNING: " << stats.warning_count << "\n";
        oss << "  - INFO: " << stats.info_count << "\n\n";

        if (violations_.empty()) {
            oss << "未发现违规!\n";
        } else {
            oss << "违规详情:\n\n";

            int index = 1;
            for (const auto& violation : violations_) {
                oss << "[" << index << "] " << ViolationSeverityToString(violation.severity) << " - "
                    << ViolationTypeToString(violation.type) << "\n";
                oss << "    位置: " << violation.file_location << ":" << violation.line_number << "\n";
                oss << "    依赖: " << GetModuleLayerName(violation.source_layer) << " (" << violation.source_module
                    << ") -> " << GetModuleLayerName(violation.target_layer) << " (" << violation.target_module
                    << ")\n";
                oss << "    描述: " << violation.description << "\n";
                oss << "    建议: " << violation.recommendation << "\n\n";
                index++;
            }
        }

        oss << "========================================\n";

        return oss.str();
    }

    /// @brief 生成简短摘要
    /// @return 摘要内容
    std::string GenerateSummary() const {
        auto stats = GetStatistics();

        std::ostringstream oss;
        oss << "架构违规统计: "
            << "Total=" << stats.total_violations << ", Critical=" << stats.critical_count
            << ", Error=" << stats.error_count << ", Warning=" << stats.warning_count << ", Info=" << stats.info_count;

        return oss.str();
    }

    // ============================================================
    // 统计和查询方法
    // ============================================================

    /// @brief 违规统计信息
    struct ViolationStatistics {
        int total_violations = 0;
        int critical_count = 0;
        int error_count = 0;
        int warning_count = 0;
        int info_count = 0;
        int invalid_dependency_count = 0;
        int circular_dependency_count = 0;
        int missing_tag_count = 0;
    };

    /// @brief 获取违规统计信息
    /// @return 统计信息
    ViolationStatistics GetStatistics() const {
        ViolationStatistics stats;
        stats.total_violations = static_cast<int>(violations_.size());

        for (const auto& violation : violations_) {
            // 按严重级别统计
            switch (violation.severity) {
                case ViolationSeverity::CRITICAL:
                    stats.critical_count++;
                    break;
                case ViolationSeverity::ERROR:
                    stats.error_count++;
                    break;
                case ViolationSeverity::WARNING:
                    stats.warning_count++;
                    break;
                case ViolationSeverity::INFO:
                    stats.info_count++;
                    break;
            }

            // 按违规类型统计
            switch (violation.type) {
                case ViolationType::INVALID_DEPENDENCY:
                    stats.invalid_dependency_count++;
                    break;
                case ViolationType::CIRCULAR_DEPENDENCY:
                    stats.circular_dependency_count++;
                    break;
                case ViolationType::MISSING_MODULE_TAG:
                    stats.missing_tag_count++;
                    break;
                default:
                    break;
            }
        }

        return stats;
    }

    /// @brief 获取所有违规记录
    /// @return 违规记录列表
    const std::vector<ViolationRecord>& GetViolations() const {
        return violations_;
    }

    /// @brief 清除所有违规记录
    void Clear() {
        violations_.clear();
    }

    /// @brief 是否存在违规
    /// @return 是否存在违规
    bool HasViolations() const {
        return !violations_.empty();
    }

    /// @brief 是否存在严重违规(ERROR或CRITICAL)
    /// @return 是否存在严重违规
    bool HasSevereViolations() const {
        for (const auto& violation : violations_) {
            if (violation.severity == ViolationSeverity::ERROR || violation.severity == ViolationSeverity::CRITICAL) {
                return true;
            }
        }
        return false;
    }

   private:
    std::vector<ViolationRecord> violations_;

    /// @brief 生成违规描述
    std::string GenerateViolationDescription(ModuleLayer source, ModuleLayer target) const {
        std::ostringstream oss;
        oss << GetModuleLayerName(source) << " layer cannot depend on " << GetModuleLayerName(target) << " layer. "
            << "This violates hexagonal architecture dependency rules.";
        return oss.str();
    }

    /// @brief 生成修复建议
    std::string GenerateRecommendation(ModuleLayer source, ModuleLayer target) const {
        std::ostringstream oss;

        if (source == ModuleLayer::DOMAIN && target != ModuleLayer::SHARED) {
            oss << "Domain layer should only depend on Shared layer interfaces. "
                << "Extract " << GetModuleLayerName(target) << " dependencies to interfaces.";
        } else if (source == ModuleLayer::INFRASTRUCTURE && target != ModuleLayer::SHARED) {
            oss << "Infrastructure layer should only depend on Shared layer interfaces. "
                << "Refactor to use dependency injection via interfaces.";
        } else {
            oss << "Review and refactor dependency to follow hexagonal architecture rules. "
                << "Ensure dependencies flow: Infrastructure->Shared, "
                << "Application->Domain->Shared.";
        }

        return oss.str();
    }
};

}  // namespace Util
}  // namespace Shared
}  // namespace Siligen
