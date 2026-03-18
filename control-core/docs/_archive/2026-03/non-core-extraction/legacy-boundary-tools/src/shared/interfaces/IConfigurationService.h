// IConfigurationService.h
// 版本: 1.0.0
// 描述: 配置管理器接口契约 - 定义配置读取、写入和管理的标准接口
//
// 六边形架构 - 端口层接口
// 依赖方向: Application层和Domain层依赖此接口获取配置

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// 前向声明 - 避免循环依赖
namespace Siligen::Shared::Types {
template <typename T>
class Result;
struct ConfigurationItem;
struct ConfigurationSection;
enum class ConfigValueType;
}  // namespace Siligen::Shared::Types

namespace Siligen::Shared::Interfaces {

/// @brief 配置管理器接口
/// @details 定义统一的配置管理接口,支持INI、JSON等多种格式
/// @note 兼容层遗留接口，新的业务代码应优先依赖 Domain::Configuration::Ports::IConfigurationPort
///
/// 性能约束:
/// - 读取操作最大时间: 5ms
/// - 写入操作最大时间: 50ms
/// - 内存开销限制: 2KB
/// - 线程安全: 支持并发读取,写操作需要独占锁
///
/// 支持的配置文件格式:
/// - INI格式 (优先支持,现有系统使用)
/// - JSON格式 (未来扩展)
///
/// 文件编码:
/// - UTF-8 (强制要求)
///
/// 使用示例:
/// @code
/// auto config = ServiceLocator::Get<IConfigurationService>();
/// config->LoadFromFile("config/machine_config.ini");
///
/// auto ip = config->GetString("connection.card_ip", "192.168.1.100");
/// auto speed = config->GetFloat("motion.max_velocity", 10.0f);
/// bool enabled = config->GetBoolean("system.auto_start", false);
/// @endcode
class IConfigurationService {
   public:
    virtual ~IConfigurationService() = default;

    // ============================================================
    // 配置加载和保存接口
    // ============================================================

    /// @brief 从文件加载配置
    /// @param file_path 配置文件路径 (支持.ini, .json, .conf)
    /// @param create_if_missing 文件不存在时是否创建 (默认false)
    /// @return Result<void> 加载结果
    /// @note 支持的文件编码: UTF-8
    virtual Siligen::Shared::Types::Result<void> LoadFromFile(const std::string& file_path,
                                                              bool create_if_missing = false) = 0;

    /// @brief 保存配置到文件
    /// @param file_path 配置文件路径
    /// @param backup_original 是否备份原文件 (默认true)
    /// @return Result<void> 保存结果
    /// @note 自动创建目录结构
    virtual Siligen::Shared::Types::Result<void> SaveToFile(const std::string& file_path,
                                                            bool backup_original = true) = 0;

    /// @brief 从文件重新加载配置
    /// @return Result<void> 重新加载结果
    /// @note 使用上次LoadFromFile的文件路径
    virtual Siligen::Shared::Types::Result<void> ReloadFromFile() = 0;

    // ============================================================
    // 通用配置项操作接口
    // ============================================================

    /// @brief 获取配置值
    /// @param key 配置键名 (支持点号分隔符,如"connection.card_ip")
    /// @param default_value 默认值 (可选)
    /// @return Result<T> 配置值
    /// @note 线程安全,并发读取
    template <typename T>
    Siligen::Shared::Types::Result<T> GetValue(const std::string& key, const T& default_value) const;

    /// @brief 设置配置值
    /// @param key 配置键名
    /// @param value 配置值
    /// @param persist 是否持久化到文件 (默认false)
    /// @return Result<void> 设置结果
    /// @note 写操作需要独占锁
    template <typename T>
    Siligen::Shared::Types::Result<void> SetValue(const std::string& key, const T& value, bool persist = false);

    /// @brief 检查配置键是否存在
    /// @param key 配置键名
    /// @return Result<bool> 是否存在
    virtual Siligen::Shared::Types::Result<bool> HasKey(const std::string& key) const = 0;

    /// @brief 删除配置项
    /// @param key 配置键名
    /// @param persist 是否从文件中删除 (默认false)
    /// @return Result<void> 删除结果
    virtual Siligen::Shared::Types::Result<void> RemoveKey(const std::string& key, bool persist = false) = 0;

    // ============================================================
    // 类型化访问接口
    // ============================================================

    /// @brief 获取字符串配置值
    /// @param key 配置键名
    /// @param default_value 默认值
    /// @return Result<string> 字符串值
    virtual Siligen::Shared::Types::Result<std::string> GetString(const std::string& key,
                                                                  const std::string& default_value = "") const = 0;

    /// @brief 获取整数配置值
    /// @param key 配置键名
    /// @param default_value 默认值
    /// @return Result<int32_t> 整数值
    virtual Siligen::Shared::Types::Result<int32_t> GetInteger(const std::string& key,
                                                               int32_t default_value = 0) const = 0;

    /// @brief 获取浮点数配置值
    /// @param key 配置键名
    /// @param default_value 默认值
    /// @return Result<float> 浮点数值
    virtual Siligen::Shared::Types::Result<float> GetFloat(const std::string& key,
                                                           float default_value = 0.0f) const = 0;

    /// @brief 获取布尔配置值
    /// @param key 配置键名
    /// @param default_value 默认值
    /// @return Result<bool> 布尔值
    /// @note 支持多种格式: true/false, 1/0, yes/no, on/off
    virtual Siligen::Shared::Types::Result<bool> GetBoolean(const std::string& key,
                                                            bool default_value = false) const = 0;

    // ============================================================
    // 批量操作接口
    // ============================================================

    /// @brief 获取配置节
    /// @param section_name 节名称
    /// @return Result<ConfigurationSection> 配置节
    /// @note 返回整个配置节及其所有子项
    virtual Siligen::Shared::Types::Result<Siligen::Shared::Types::ConfigurationSection> GetSection(
        const std::string& section_name) const = 0;

    /// @brief 获取所有配置键
    /// @param pattern 键名模式过滤器 (可选,支持通配符)
    /// @return Result<vector<string>> 键名列表
    virtual Siligen::Shared::Types::Result<std::vector<std::string>> GetAllKeys(
        const std::string& pattern = "") const = 0;

    /// @brief 清空所有配置
    /// @return Result<void> 清空结果
    /// @note 仅清空内存中的配置,不影响文件
    virtual Siligen::Shared::Types::Result<void> Clear() = 0;

    // ============================================================
    // 配置验证接口
    // ============================================================

    /// @brief 验证配置完整性
    /// @return Result<ValidationResult> 验证结果
    /// @note 检查必需配置项、类型正确性、值范围等
    virtual Siligen::Shared::Types::Result<bool> ValidateConfiguration() const = 0;

    /// @brief 获取验证错误信息
    /// @return Result<vector<string>> 错误信息列表
    virtual Siligen::Shared::Types::Result<std::vector<std::string>> GetValidationErrors() const = 0;

    /// @brief 获取验证警告信息
    /// @return Result<vector<string>> 警告信息列表
    virtual Siligen::Shared::Types::Result<std::vector<std::string>> GetValidationWarnings() const = 0;

    // ============================================================
    // 配置备份和恢复接口
    // ============================================================

    /// @brief 创建配置备份
    /// @param backup_name 备份名称 (可选,默认使用时间戳)
    /// @return Result<void> 备份结果
    virtual Siligen::Shared::Types::Result<void> CreateBackup(const std::string& backup_name = "") = 0;

    /// @brief 恢复配置备份
    /// @param backup_name 备份名称
    /// @return Result<void> 恢复结果
    virtual Siligen::Shared::Types::Result<void> RestoreBackup(const std::string& backup_name) = 0;

    /// @brief 列出所有备份
    /// @return Result<vector<BackupInfo>> 备份信息列表
    /// @note 包含备份名称、时间戳、文件大小等信息
    virtual Siligen::Shared::Types::Result<std::vector<std::string>> ListBackups() const = 0;

    // ============================================================
    // 接口元数据
    // ============================================================

    /// @brief 获取接口版本号
    /// @return 版本字符串 (语义化版本)
    virtual const char* GetInterfaceVersion() const noexcept {
        return "1.0.0";
    }

    /// @brief 获取实现类型名称
    /// @return 实现类型名称
    /// @note 用于调试和日志记录
    virtual const char* GetImplementationType() const noexcept = 0;

    /// @brief 获取当前配置文件路径
    /// @return 配置文件路径
    virtual const char* GetConfigFilePath() const noexcept = 0;

    /// @brief 获取支持的配置文件格式列表
    /// @return 格式列表 (如: "ini, json")
    virtual const char* GetSupportedFormats() const noexcept {
        return "ini, json";
    }
};

// ============================================================
// 便捷访问宏定义
// ============================================================

// 获取配置值的便捷宏
#define CONFIG_GET_STRING(key, default_val) \
    Siligen::Shared::Interfaces::IConfigurationService::GetInstance()->GetString(key, default_val)

#define CONFIG_GET_INT(key, default_val) \
    Siligen::Shared::Interfaces::IConfigurationService::GetInstance()->GetInteger(key, default_val)

#define CONFIG_GET_FLOAT(key, default_val) \
    Siligen::Shared::Interfaces::IConfigurationService::GetInstance()->GetFloat(key, default_val)

#define CONFIG_GET_BOOL(key, default_val) \
    Siligen::Shared::Interfaces::IConfigurationService::GetInstance()->GetBoolean(key, default_val)

}  // namespace Siligen::Shared::Interfaces
