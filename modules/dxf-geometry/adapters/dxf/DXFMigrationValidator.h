#pragma once

#include "DXFMigrationConfig.h"
#include "AutoPathSourceAdapter.h"
#include <string>
#include <vector>

namespace Siligen::Infrastructure::Adapters::Parsing {

/**
 * @brief DXF迁移验证工具
 * 
 * 提供迁移功能的测试和验证，确保迁移过程可靠。
 */
struct MigrationTestResult {
    std::string test_name;
    bool success = false;
    std::string error_message;
    std::string details;
    int execution_time_ms = 0;
};

class DXFMigrationValidator {
public:
    /**
     * @brief 运行完整的迁移验证测试
     */
    static std::vector<MigrationTestResult> RunFullValidation();
    
    /**
     * @brief 验证服务可用性
     */
    static MigrationTestResult ValidateServiceAvailability();
    
    /**
     * @brief 验证命令行接口
     */
    static MigrationTestResult ValidateCommandLineInterface();
    
    /**
     * @brief 验证文件处理功能
     */
    static MigrationTestResult ValidateFileProcessing();
    
    /**
     * @brief 验证错误处理机制
     */
    static MigrationTestResult ValidateErrorHandling();
    
    /**
     * @brief 验证性能表现
     */
    static MigrationTestResult ValidatePerformance();
    
    /**
     * @brief 生成验证报告
     */
    static std::string GenerateValidationReport(const std::vector<MigrationTestResult>& results);
    
    /**
     * @brief 检查是否所有测试都通过
     */
    static bool AllTestsPassed(const std::vector<MigrationTestResult>& results);
    
    /**
     * @brief 获取建议的迁移阶段
     */
    static DXFMigrationConfig::MigrationPhase GetRecommendedMigrationPhase(
        const std::vector<MigrationTestResult>& results);

private:
    // 测试文件路径
    static const std::string TEST_DXF_FILE;
    static const std::string TEST_OUTPUT_DIR;
    
    // 创建测试文件
    static bool CreateTestDXFFile();
    static bool CleanupTestFiles();
    
    // 性能测试
    static MigrationTestResult RunPerformanceTest();
    
    // 错误处理测试
    static MigrationTestResult RunErrorHandlingTest();
};

} // namespace Siligen::Infrastructure::Adapters::Parsing