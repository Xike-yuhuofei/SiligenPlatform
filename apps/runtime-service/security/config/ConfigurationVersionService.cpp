// 配置文件版本控制器实现
// 功能: 管理配置文件版本兼容性和迁移
// 约束: 核心文件≤200行

#include "ConfigurationVersionService.h"

#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>

// 简化版本：不使用filesystem，避免clang-cl兼容性问题
// 直接使用C++标准文件流操作

namespace Siligen {
namespace Application {

// 解析版本字符串
Version Version::Parse(const std::string& version_str) {
    Version v;
    std::regex version_regex(R"((\d+)\.(\d+)\.(\d+))");
    std::smatch match;

    if (std::regex_match(version_str, match, version_regex)) {
        v.major = std::stoi(match[1].str());
        v.minor = std::stoi(match[2].str());
        v.patch = std::stoi(match[3].str());
    }

    return v;
}

// 版本转字符串
std::string Version::ToString() const {
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

// 版本比较
bool Version::operator<(const Version& other) const {
    if (major != other.major) return major < other.major;
    if (minor != other.minor) return minor < other.minor;
    return patch < other.patch;
}

bool Version::operator==(const Version& other) const {
    return major == other.major && minor == other.minor && patch == other.patch;
}

// 版本兼容性检查 (主版本号必须相同)
bool Version::IsCompatible(const Version& required) const {
    // 主版本号必须相同,且当前版本不低于需求版本
    if (major != required.major) return false;
    return !(*this < required);  // 等价于 >=
}

// 验证版本兼容性
bool ConfigurationVersionService::ValidateVersion(const std::string& config_version, const std::string& required_version) {
    auto current = Version::Parse(config_version);
    auto required = Version::Parse(required_version);

    if (!current.IsCompatible(required)) {
        std::ostringstream oss;
        oss << "配置文件版本不兼容 (当前: " << current.ToString() << ", 需要: " << required.ToString() << ")";
        last_error_ = oss.str();
        return false;
    }

    return true;
}

// 生成时间戳
std::string ConfigurationVersionService::GenerateTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &time_t);  // Windows安全版本

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return oss.str();
}

// 获取备份路径
std::string ConfigurationVersionService::GetBackupPath(const std::string& config_path) const {
    // 简化版本：手动解析路径
    size_t last_slash = config_path.find_last_of("/\\");
    size_t last_dot = config_path.find_last_of('.');

    std::string directory = (last_slash != std::string::npos) ? config_path.substr(0, last_slash + 1) : "";
    std::string filename = (last_slash != std::string::npos) ? config_path.substr(last_slash + 1) : config_path;
    std::string extension =
        (last_dot != std::string::npos && last_dot > last_slash) ? config_path.substr(last_dot) : "";

    // 移除扩展名
    if (!extension.empty()) {
        filename = filename.substr(0, filename.length() - extension.length());
    }

    std::string backup_name = filename + "_backup_" + GenerateTimestamp() + extension;
    return directory + backup_name;
}

// 备份配置文件
bool ConfigurationVersionService::BackupConfig(const std::string& config_path) {
    // 简化版本：不使用异常，直接返回错误
    std::ifstream input_file(config_path);
    if (!input_file.good()) {
        last_error_ = "配置文件不存在或无法访问: " + config_path;
        return false;
    }

    std::string backup_path = GetBackupPath(config_path);
    std::ofstream output_file(backup_path, std::ios::binary);
    if (!output_file.good()) {
        last_error_ = "无法创建备份文件: " + backup_path;
        return false;
    }

    // 复制文件内容
    output_file << input_file.rdbuf();
    if (!output_file.good()) {
        last_error_ = "备份文件写入失败: " + backup_path;
        return false;
    }

    return true;
}

}  // namespace Application
}  // namespace Siligen
