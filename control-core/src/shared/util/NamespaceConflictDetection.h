// NamespaceConflictDetection.h
// Phase 3: T041 - 命名空间冲突检测和编译时检查机制
// 说明: 此文件提供编译时和运行时的命名冲突检测机制
//
// 使用方法:
// 1. 在需要检测冲突的源文件中包含此头文件
// 2. 编译时会自动检测常见的宏冲突
// 3. 可选: 调用DetectNamespaceConflicts()进行运行时检查

#pragma once

#include "../types/CMPTypes.h"

#include <string>
#include <vector>

namespace Siligen {
namespace Shared {
namespace Util {

// ============================================================
// 编译时宏冲突检测 (Compile-time macro conflict detection)
// ============================================================

// 检测LOG_*宏冲突 (Detect LOG_* macro conflicts)
#if defined(LOG_INFO) && !defined(SILIGEN_LOG_INFO)
#error \
    "LOG_INFO macro conflict detected! Use SILIGEN_LOG_INFO instead. Remove old LOG_INFO definition from utils/Logger.h or hardware/StatusMonitor.h"
#endif

#if defined(LOG_ERROR) && !defined(SILIGEN_LOG_ERROR)
#error \
    "LOG_ERROR macro conflict detected! Use SILIGEN_LOG_ERROR instead. Remove old LOG_ERROR definition from utils/Logger.h or hardware/StatusMonitor.h"
#endif

#if defined(LOG_WARNING) && !defined(SILIGEN_LOG_WARNING)
#error \
    "LOG_WARNING macro conflict detected! Use SILIGEN_LOG_WARNING instead. Remove old LOG_WARNING definition from utils/Logger.h or hardware/StatusMonitor.h"
#endif

#if defined(LOG_DEBUG) && !defined(SILIGEN_LOG_DEBUG)
#error \
    "LOG_DEBUG macro conflict detected! Use SILIGEN_LOG_DEBUG instead. Remove old LOG_DEBUG definition from utils/Logger.h or hardware/StatusMonitor.h"
#endif

// ============================================================
// 编译时类型冲突检测 (Compile-time type conflict detection)
// ============================================================

// 验证统一的CMPTypes命名空间 (Verify unified CMPTypes namespace)
namespace _ConflictDetection {
// 此命名空间仅用于编译时检查，不应在实际代码中使用
// This namespace is only for compile-time checks, should not be used in actual code

// 强制检查CMPTriggerMode是否在正确的命名空间下
// Force check if CMPTriggerMode is in the correct namespace
using TestCMPTriggerMode = ::Siligen::Shared::Types::CMPTriggerMode;
using TestCMPConfiguration = ::Siligen::Shared::Types::CMPConfiguration;
using TestCMPTriggerPoint = ::Siligen::Shared::Types::CMPTriggerPoint;
using TestCMPStatus = ::Siligen::Shared::Types::CMPStatus;
using TestCMPSignalType = ::Siligen::Shared::Types::CMPSignalType;
using TestDispensingAction = ::Siligen::Shared::Types::DispensingAction;

// 验证向后兼容别名 (Verify backward compatibility aliases)
static_assert(std::is_same_v<TestCMPTriggerMode, ::Siligen::CMPTriggerMode>,
              "CMPTriggerMode backward compatibility alias is broken");
static_assert(std::is_same_v<TestDispensingAction, ::Siligen::DispensingAction>,
              "DispensingAction backward compatibility alias is broken");
}  // namespace _ConflictDetection

// ============================================================
// 运行时冲突检测 (Runtime conflict detection)
// ============================================================

/**
 * @brief 命名空间冲突检测报告
 */
struct ConflictReport {
    std::vector<std::string> macro_conflicts;   // 宏冲突列表
    std::vector<std::string> type_conflicts;    // 类型冲突列表
    std::vector<std::string> namespace_issues;  // 命名空间问题列表
    bool has_conflicts;                         // 是否有冲突

    ConflictReport() : has_conflicts(false) {}

    // 添加冲突 (Add conflict)
    void AddMacroConflict(const std::string& macro_name) {
        macro_conflicts.push_back(macro_name);
        has_conflicts = true;
    }

    void AddTypeConflict(const std::string& type_name) {
        type_conflicts.push_back(type_name);
        has_conflicts = true;
    }

    void AddNamespaceIssue(const std::string& issue) {
        namespace_issues.push_back(issue);
        has_conflicts = true;
    }

    // 生成报告字符串 (Generate report string)
    std::string ToString() const {
        std::string report = "[Namespace Conflict Detection Report]\n";

        if (!has_conflicts) {
            report += "✓ No conflicts detected\n";
            return report;
        }

        if (!macro_conflicts.empty()) {
            report += "\n✗ Macro Conflicts:\n";
            for (const auto& conflict : macro_conflicts) {
                report += "  - " + conflict + "\n";
            }
        }

        if (!type_conflicts.empty()) {
            report += "\n✗ Type Conflicts:\n";
            for (const auto& conflict : type_conflicts) {
                report += "  - " + conflict + "\n";
            }
        }

        if (!namespace_issues.empty()) {
            report += "\n✗ Namespace Issues:\n";
            for (const auto& issue : namespace_issues) {
                report += "  - " + issue + "\n";
            }
        }

        return report;
    }
};

/**
 * @brief 运行时命名空间冲突检测
 * @return 冲突检测报告
 */
inline ConflictReport DetectNamespaceConflicts() {
    ConflictReport report;

// 检测宏冲突 (Detect macro conflicts)
#if defined(LOG_INFO) && !defined(SILIGEN_LOG_INFO)
    report.AddMacroConflict("LOG_INFO is defined but SILIGEN_LOG_INFO is not");
#endif

#if defined(LOG_ERROR) && !defined(SILIGEN_LOG_ERROR)
    report.AddMacroConflict("LOG_ERROR is defined but SILIGEN_LOG_ERROR is not");
#endif

#if defined(LOG_WARNING) && !defined(SILIGEN_LOG_WARNING)
    report.AddMacroConflict("LOG_WARNING is defined but SILIGEN_LOG_WARNING is not");
#endif

#if defined(LOG_DEBUG) && !defined(SILIGEN_LOG_DEBUG)
    report.AddMacroConflict("LOG_DEBUG is defined but SILIGEN_LOG_DEBUG is not");
#endif

    // 验证统一的CMPTypes路径
    // Verify unified CMPTypes path
    // 注: 这些检查在编译时已完成，运行时主要用于日志记录

    return report;
}

// ============================================================
// 使用指南 (Usage Guide)
// ============================================================

/*
使用方法 (Usage):

1. 编译时检测 (Compile-time detection):
   只需在源文件中包含此头文件，编译器会自动检测冲突
   Just include this header in your source file, compiler will detect conflicts automatically

   #include "shared/util/NamespaceConflictDetection.h"

2. 运行时检测 (Runtime detection):
   在程序启动时调用检测函数并输出报告
   Call detection function at program startup and output report

   #include "shared/util/NamespaceConflictDetection.h"

   #include <iostream>

   int main() {
       auto report = Siligen::Shared::Util::DetectNamespaceConflicts();
       if (report.has_conflicts) {
           std::cerr << report.ToString();
           return 1;
       }
       std::cout << "✓ No namespace conflicts detected\n";
       // ... rest of your program
   }

3. 单元测试 (Unit testing):
   在单元测试中验证无冲突状态
   Verify no-conflict state in unit tests

   TEST(NamespaceTest, NoConflicts) {
       auto report = Siligen::Shared::Util::DetectNamespaceConflicts();
       ASSERT_FALSE(report.has_conflicts);
   }
*/

}  // namespace Util
}  // namespace Shared
}  // namespace Siligen
