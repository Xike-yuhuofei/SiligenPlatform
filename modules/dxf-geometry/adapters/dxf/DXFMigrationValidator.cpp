#include "DXFMigrationValidator.h"

#include "shared/types/Error.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace Siligen::Infrastructure::Adapters::Parsing {

// 静态成员初始化
const std::string DXFMigrationValidator::TEST_DXF_FILE = "test_migration.dxf";
const std::string DXFMigrationValidator::TEST_OUTPUT_DIR = "test_output";

std::vector<MigrationTestResult> DXFMigrationValidator::RunFullValidation() {
    std::vector<MigrationTestResult> results;
    
    // 清理之前的测试文件
    CleanupTestFiles();
    
    // 创建测试目录
    std::filesystem::create_directories(TEST_OUTPUT_DIR);
    
    // 运行各项测试
    results.push_back(ValidateServiceAvailability());
    results.push_back(ValidateCommandLineInterface());
    
    // 只有在服务可用时才运行文件处理测试
    if (DXFMigrationConfigManager::IsServiceAvailable()) {
        if (CreateTestDXFFile()) {
            results.push_back(ValidateFileProcessing());
            results.push_back(RunPerformanceTest());
            results.push_back(RunErrorHandlingTest());
        } else {
            MigrationTestResult file_result;
            file_result.test_name = "文件处理测试";
            file_result.success = false;
            file_result.error_message = "无法创建测试文件";
            results.push_back(file_result);
        }
    }
    
    // 清理测试文件
    CleanupTestFiles();
    
    return results;
}

MigrationTestResult DXFMigrationValidator::ValidateServiceAvailability() {
    MigrationTestResult result;
    result.test_name = "服务可用性验证";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        bool is_available = DXFMigrationConfigManager::IsServiceAvailable();
        bool config_valid = DXFMigrationConfigManager::ValidateConfig();
        
        result.success = is_available && config_valid;
        
        if (is_available) {
            result.details = "DXF pipeline服务可用，配置验证通过";
        } else {
            result.details = "DXF pipeline服务不可用或配置验证失败";
        }
        
        if (!config_valid) {
            result.error_message = "配置验证失败";
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "验证过程中发生异常: " + std::string(e.what());
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.execution_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    return result;
}

MigrationTestResult DXFMigrationValidator::ValidateCommandLineInterface() {
    MigrationTestResult result;
    result.test_name = "命令行接口验证";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        auto& config = DXFMigrationConfigManager::GetConfig();
        
        if (config.service_protocol != DXFMigrationConfig::ServiceProtocol::COMMAND_LINE) {
            result.success = true;
            result.details = "当前配置不使用命令行接口，跳过验证";
        } else {
            // 测试命令行构建功能
            std::string command = DXFMigrationConfigManager::GetServiceCommand("test.dxf", "test.pb");
            
            result.success = !command.empty();
            
            if (result.success) {
                result.details = "命令行构建成功: " + command;
            } else {
                result.error_message = "无法构建有效的命令行";
            }
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "验证过程中发生异常: " + std::string(e.what());
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.execution_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    return result;
}

MigrationTestResult DXFMigrationValidator::ValidateFileProcessing() {
    MigrationTestResult result;
    result.test_name = "文件处理功能验证";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // 创建测试适配器
        AutoPathSourceAdapter adapter;
        
        // 测试文件验证
        auto validation_result = adapter.ValidateDXFFile(TEST_DXF_FILE);
        
        if (validation_result.IsSuccess()) {
            const auto& validation_data = validation_result.Value();
            result.success = validation_data.is_valid;
            
            if (result.success) {
                result.details = "DXF文件验证通过，格式: " + validation_data.format_version;
            } else {
                result.error_message = "DXF文件验证失败: " + validation_data.error_details;
            }
        } else {
            result.success = false;
            result.error_message = "文件验证过程失败: " + validation_result.GetError().GetMessage();
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "验证过程中发生异常: " + std::string(e.what());
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.execution_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    return result;
}

MigrationTestResult DXFMigrationValidator::RunPerformanceTest() {
    MigrationTestResult result;
    result.test_name = "性能测试";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // 简单的性能测试：多次调用验证功能
        AutoPathSourceAdapter adapter;
        int iterations = 10;
        int success_count = 0;
        
        for (int i = 0; i < iterations; ++i) {
            auto validation_result = adapter.ValidateDXFFile(TEST_DXF_FILE);
            if (validation_result.IsSuccess() && validation_result.Value().is_valid) {
                success_count++;
            }
        }
        
        result.success = success_count == iterations;
        result.details = "性能测试完成，成功率: " + std::to_string(success_count) + "/" + std::to_string(iterations);
        
        if (!result.success) {
            result.error_message = "性能测试中发现失败调用";
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "性能测试过程中发生异常: " + std::string(e.what());
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.execution_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    return result;
}

MigrationTestResult DXFMigrationValidator::RunErrorHandlingTest() {
    MigrationTestResult result;
    result.test_name = "错误处理测试";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        AutoPathSourceAdapter adapter;
        
        // 测试不存在的文件
        auto invalid_result = adapter.ValidateDXFFile("nonexistent_file.dxf");
        
        // 期望验证失败
        if (invalid_result.IsSuccess()) {
            // 如果文件不存在但验证成功，说明有问题
            const auto& validation_data = invalid_result.Value();
            result.success = !validation_data.is_valid;
            
            if (result.success) {
                result.details = "错误处理正常，无效文件被正确识别";
            } else {
                result.error_message = "错误处理异常，无效文件被误判为有效";
            }
        } else {
            // 验证过程本身失败，这也是可以接受的错误处理
            result.success = true;
            result.details = "错误处理正常，验证过程正确失败";
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "错误处理测试过程中发生异常: " + std::string(e.what());
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.execution_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    return result;
}

std::string DXFMigrationValidator::GenerateValidationReport(const std::vector<MigrationTestResult>& results) {
    std::stringstream report;
    
    report << "=== DXF迁移验证报告 ===\n";
    report << "生成时间: " << __DATE__ << " " << __TIME__ << "\n\n";
    
    int total_tests = results.size();
    int passed_tests = 0;
    
    for (const auto& result : results) {
        report << "测试: " << result.test_name << "\n";
        report << "状态: " << (result.success ? "✅ 通过" : "❌ 失败") << "\n";
        report << "耗时: " << result.execution_time_ms << "ms\n";
        
        if (!result.details.empty()) {
            report << "详情: " << result.details << "\n";
        }
        
        if (!result.error_message.empty()) {
            report << "错误: " << result.error_message << "\n";
        }
        
        report << "---\n";
        
        if (result.success) {
            passed_tests++;
        }
    }
    
    report << "\n总结: " << passed_tests << "/" << total_tests << " 测试通过\n";
    
    DXFMigrationConfig::MigrationPhase recommended_phase = GetRecommendedMigrationPhase(results);
    const char* phase_names[] = {"LOCAL_ONLY", "SERVICE_AVAILABLE", "SERVICE_REQUIRED"};
    
    report << "建议迁移阶段: " << phase_names[static_cast<int>(recommended_phase)] << "\n";
    
    return report.str();
}

bool DXFMigrationValidator::AllTestsPassed(const std::vector<MigrationTestResult>& results) {
    for (const auto& result : results) {
        if (!result.success) {
            return false;
        }
    }
    return true;
}

DXFMigrationConfig::MigrationPhase DXFMigrationValidator::GetRecommendedMigrationPhase(
    const std::vector<MigrationTestResult>& results) {
    
    if (results.empty()) {
        return DXFMigrationConfig::MigrationPhase::LOCAL_ONLY;
    }
    
    bool all_passed = AllTestsPassed(results);
    bool service_available = DXFMigrationConfigManager::IsServiceAvailable();
    
    if (all_passed && service_available) {
        return DXFMigrationConfig::MigrationPhase::SERVICE_AVAILABLE;
    } else if (all_passed) {
        // 测试通过但服务不可用，建议保持本地模式
        return DXFMigrationConfig::MigrationPhase::LOCAL_ONLY;
    } else {
        // 有测试失败，建议保持本地模式
        return DXFMigrationConfig::MigrationPhase::LOCAL_ONLY;
    }
}

bool DXFMigrationValidator::CreateTestDXFFile() {
    try {
        std::ofstream test_file(TEST_DXF_FILE);
        if (!test_file) {
            return false;
        }
        
        // 创建简单的测试DXF内容
        test_file << "0\nSECTION\n";
        test_file << "2\nHEADER\n";
        test_file << "9\n$ACADVER\n";
        test_file << "1\nAC1009\n";  // R12版本
        test_file << "0\nENDSEC\n";
        test_file << "0\nSECTION\n";
        test_file << "2\nENTITIES\n";
        test_file << "0\nLINE\n";
        test_file << "8\n0\n";  // 图层
        test_file << "10\n0.0\n";  // 起点X
        test_file << "20\n0.0\n";  // 起点Y
        test_file << "11\n100.0\n";  // 终点X
        test_file << "21\n100.0\n";  // 终点Y
        test_file << "0\nENDSEC\n";
        test_file << "0\nEOF\n";
        
        test_file.close();
        return true;
        
    } catch (...) {
        return false;
    }
}

bool DXFMigrationValidator::CleanupTestFiles() {
    try {
        if (std::filesystem::exists(TEST_DXF_FILE)) {
            std::filesystem::remove(TEST_DXF_FILE);
        }
        
        if (std::filesystem::exists(TEST_OUTPUT_DIR)) {
            std::filesystem::remove_all(TEST_OUTPUT_DIR);
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace Siligen::Infrastructure::Adapters::Parsing
