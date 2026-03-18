#pragma once

#include "domain/configuration/ports/IConfigurationPort.h"
#include "shared/types/CMPTypes.h"
#include "shared/types/Types.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Siligen {

// 使用域端口中的类型定义
using DispensingParameters = Siligen::Domain::Configuration::Ports::DispensingConfig;
using MachineParameters = Siligen::Domain::Configuration::Ports::MachineConfig;
using HomingConfig = Siligen::Domain::Configuration::Ports::HomingConfig;

/**
 * @brief 阀门时序参数
 */
struct ValveTimingParams {
    float32 open_delay = 0.001f;           // 开启延时 (s)
    float32 close_delay = 0.001f;          // 关闭延时 (s)
    float32 min_on_time = 0.01f;           // 最小开启时间 (s)
    float32 max_on_time = 10.0f;           // 最大开启时间 (s)
    float32 valve_response_time = 0.005f;  // 阀门响应时间 (s)

    // 验证参数有效性
    bool IsValid() const;

    // 重置为默认值
    void ResetToDefaults();
};

/**
 * @brief CMP脉冲配置
 */
struct CMPPulseConfig {
    int32 cmp_channel = 1;  // CMP通道
    Shared::Types::CMPSignalType signal_type = Shared::Types::CMPSignalType::PULSE;
    Shared::Types::CMPTriggerMode trigger_mode = Shared::Types::CMPTriggerMode::SINGLE_POINT;
    int32 pulse_width_us = 2000;     // 脉冲宽度 (μs)
    int32 delay_time_us = 0;         // 延时时间 (μs)
    int32 encoder_num = 1;           // 编码器编号
    bool abs_position_flag = false;  // 绝对位置标志

    // 验证参数有效性
    bool IsValid() const;

    // 重置为默认值
    void ResetToDefaults();
};

/**
 * @brief 网络配置
 */
struct NetworkConfig {
    std::string control_card_ip = CONTROL_CARD_IP;
    std::string local_ip = LOCAL_IP;
    uint16 control_card_port = CONTROL_CARD_PORT;
    uint16 local_port = LOCAL_PORT;
    int32 timeout_ms = CONNECTION_TIMEOUT;

    // 验证参数有效性
    bool IsValid() const;

    // 重置为默认值
    void ResetToDefaults();
};

/**
 * @brief 配置管理器类
 * 负责INI配置文件的读取、写入和管理
 */
class ConfigurationService {
   public:
    /**
     * @brief 构造函数
     * @param config_file 配置文件路径
     */
    explicit ConfigurationService(const std::string& config_file);

    /**
     * @brief 析构函数
     */
    ~ConfigurationService() = default;

    // 禁止拷贝，允许移动
    ConfigurationService(const ConfigurationService&) = delete;
    ConfigurationService& operator=(const ConfigurationService&) = delete;
    ConfigurationService(ConfigurationService&&) = default;
    ConfigurationService& operator=(ConfigurationService&&) = default;

    /**
     * @brief 加载配置文件
     * @return 是否加载成功
     */
    bool LoadConfig();

    /**
     * @brief 保存配置文件
     * @return 是否保存成功
     */
    bool SaveConfig() const;

    /**
     * @brief 重新加载配置文件
     * @return 是否重新加载成功
     */
    bool ReloadConfig();

    // 获取配置参数的方法
    const DispensingParameters& GetDispensingParams() const {
        return dispensing_params_;
    }
    const MachineParameters& GetMachineParams() const {
        return machine_params_;
    }
    const ValveTimingParams& GetValveTimingParams() const {
        return valve_timing_params_;
    }
    const CMPPulseConfig& GetCMPConfig() const {
        return cmp_config_;
    }
    const NetworkConfig& GetNetworkConfig() const {
        return network_config_;
    }

    // === 回零配置管理 ===
    const HomingConfig& GetHomingConfig(int axis) const;
    void SetHomingConfig(int axis, const HomingConfig& config);
    const std::vector<HomingConfig>& GetAllHomingConfigs() const;
    void SetAllHomingConfigs(const std::vector<HomingConfig>& configs);

    // 设置配置参数的方法
    void SetDispensingParams(const DispensingParameters& params);
    void SetMachineParams(const MachineParameters& params);
    void SetValveTimingParams(const ValveTimingParams& params);
    void SetCMPConfig(const CMPPulseConfig& config);
    void SetNetworkConfig(const NetworkConfig& config);

    // 单个参数设置方法
    bool SetMachineSpeed(float32 max_speed);
    bool SetMachineAcceleration(float32 max_acceleration);
    bool SetWorkAreaSize(float32 width, float32 height);
    bool SetDispensingTime(float32 time);
    bool SetDispensingPressure(float32 pressure);

    // 配置验证
    bool ValidateConfig() const;
    std::vector<std::string> GetValidationErrors() const;

    // 配置重置
    void ResetToDefaults();
    void ResetDispensingToDefaults();
    void ResetMachineToDefaults();

    // 配置文件操作
    bool BackupConfig(const std::string& backup_file) const;
    bool RestoreConfig(const std::string& backup_file);
    bool ExportToJSON(const std::string& json_file) const;
    bool ImportFromJSON(const std::string& json_file);

    // 获取配置文件路径
    const std::string& GetConfigFilePath() const {
        return config_file_;
    }

    // 设置配置文件路径
    void SetConfigFilePath(const std::string& file_path);

    // 检查配置文件是否存在
    bool ConfigFileExists() const;

    // 获取配置文件的最后修改时间
    int64 GetConfigFileLastModified() const;

   private:
    std::string config_file_;  // 配置文件路径

    // 配置参数
    DispensingParameters dispensing_params_;
    MachineParameters machine_params_;
    ValveTimingParams valve_timing_params_;
    CMPPulseConfig cmp_config_;
    NetworkConfig network_config_;

    // 回零配置（4个轴）
    std::vector<HomingConfig> homing_configs_;

    // 内部方法
    bool ReadIniFile();
    bool WriteIniFile() const;
    std::string ReadIniValue(const std::string& section,
                             const std::string& key,
                             const std::string& default_value = "") const;
    bool WriteIniValue(const std::string& section, const std::string& key, const std::string& value) const;

    // 读取各个配置段
    void ReadDispensingSection();
    void ReadMachineSection();
    void ReadValveTimingSection();
    void ReadCMPSection();
    void ReadNetworkSection();

    // 写入各个配置段
    void WriteDispensingSection() const;
    void WriteMachineSection() const;
    void WriteValveTimingSection() const;
    void WriteCMPSection() const;
    void WriteNetworkSection() const;

    // 回零配置段操作
    void ReadHomingSection();
    void WriteHomingSection() const;
    std::string GetHomingAxisSectionName(int axis) const;

    // 辅助方法
    float32 StringToFloat(const std::string& str, float32 default_value = 0.0f) const;
    int32 StringToInt(const std::string& str, int32 default_value = 0) const;
    bool StringToBool(const std::string& str, bool default_value = false) const;
    std::string FloatToString(float32 value) const;
    std::string IntToString(int32 value) const;
    std::string BoolToString(bool value) const;
};

}  // namespace Siligen




