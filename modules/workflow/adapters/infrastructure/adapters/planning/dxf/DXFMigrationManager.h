#pragma once

#include "DXFMigrationConfig.h"
#include "DXFMigrationValidator.h"
#include <functional>
#include <string>
#include <vector>

namespace Siligen::Infrastructure::Adapters::Parsing {

/**
 * @brief DXF迁移管理器
 * 
 * 提供迁移操作的管理和执行功能。
 */
struct MigrationOperation {
    std::string operation_name;
    std::string description;
    bool required = true;
    std::function<bool()> execute_function;
};

class DXFMigrationManager {
public:
    /**
     * @brief 执行完整的迁移流程
     */
    static bool ExecuteFullMigration();
    
    /**
     * @brief 执行预迁移验证
     */
    static bool ExecutePreMigrationValidation();
    
    /**
     * @brief 执行迁移操作
     */
    static bool ExecuteMigrationOperations();
    
    /**
     * @brief 执行后迁移验证
     */
    static bool ExecutePostMigrationValidation();
    
    /**
     * @brief 回滚迁移操作
     */
    static bool RollbackMigration();
    
    /**
     * @brief 获取迁移操作列表
     */
    static std::vector<MigrationOperation> GetMigrationOperations();
    
    /**
     * @brief 生成迁移报告
     */
    static std::string GenerateMigrationReport(bool success, const std::vector<MigrationTestResult>& validation_results);
    
    /**
     * @brief 更新迁移配置
     */
    static bool UpdateMigrationConfiguration(DXFMigrationConfig::MigrationPhase new_phase);
    
    /**
     * @brief 检查迁移状态
     */
    static std::string GetMigrationStatus();

private:
    // 迁移操作实现
    static bool MigrateScriptFiles();
    static bool UpdateConfigurationFiles();
    static bool UpdateBuildSystem();
    static bool UpdateDocumentation();
    static bool TestMigrationResults();
    
    // 回滚操作实现
    static bool RollbackScriptFiles();
    static bool RollbackConfiguration();
    static bool RollbackBuildSystem();
    
    // 工具方法
    static bool CopyFileWithBackup(const std::string& source, const std::string& destination);
    static bool CreateBackup(const std::string& file_path);
    static bool RestoreBackup(const std::string& file_path);
};

} // namespace Siligen::Infrastructure::Adapters::Parsing
