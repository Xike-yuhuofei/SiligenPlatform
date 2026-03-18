#pragma once
// 配置文件版本控制器
// 功能: 管理配置文件版本兼容性和迁移
// 约束: 代码≤100行(头文件限制)

#include <chrono>
#include <string>

namespace Siligen {
namespace Application {

// 版本结构
struct Version {
    int major = 0;  // 主版本号 (不兼容变更)
    int minor = 0;  // 次版本号 (向后兼容)
    int patch = 0;  // 修订版本号 (bug修复)

    // 从字符串解析版本 (格式: "1.0.0")
    static Version Parse(const std::string& version_str);

    // 转换为字符串
    std::string ToString() const;

    // 版本比较
    bool operator<(const Version& other) const;
    bool operator==(const Version& other) const;
    bool IsCompatible(const Version& required) const;  // 检查是否兼容
};

// 配置版本管理器
class ConfigurationVersionService {
   public:
    ConfigurationVersionService() = default;
    ~ConfigurationVersionService() = default;

    // 验证配置文件版本兼容性
    // 返回: true=兼容, false=不兼容
    bool ValidateVersion(const std::string& config_version, const std::string& required_version);

    // 备份配置文件 (迁移前)
    // 返回: true=备份成功, false=备份失败
    bool BackupConfig(const std::string& config_path);

    // 获取备份路径
    std::string GetBackupPath(const std::string& config_path) const;

    // 最后错误信息
    std::string GetLastError() const {
        return last_error_;
    }

   private:
    std::string last_error_;  // 最后错误信息

    // 生成时间戳后缀 (格式: YYYYMMDD_HHMMSS)
    std::string GenerateTimestamp() const;
};

}  // namespace Application
}  // namespace Siligen
