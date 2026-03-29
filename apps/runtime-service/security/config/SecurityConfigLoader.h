#pragma once

#include "shared/types/Types.h"
#include "ConfigurationVersionService.h"

#include <optional>
#include <string>
#include <vector>

namespace Siligen {

// 安全配置结构
struct SecurityConfig {
    // 版本信息
    std::string version;

    // 会话管理
    int32 session_timeout_minutes = 30;   // 会话超时时间(分钟)
    int32 max_failed_login_attempts = 5;  // 最大登录失败次数
    int32 lockout_duration_minutes = 15;  // 账号锁定时间(分钟)

    // 密码策略
    int32 min_password_length = 8;       // 最小密码长度
    bool require_mixed_case = true;      // 要求大小写混合
    bool require_numbers = true;         // 要求包含数字
    bool require_special_chars = false;  // 要求特殊字符

    // 运动安全限制
    float32 max_speed_mm_s = 10.0f;          // 最大速度(mm/s)
    float32 max_acceleration_mm_s2 = 10.0f;  // 最大加速度(mm/s²)
    float32 max_jerk_mm_s3 = 100.0f;         // 最大加加速度(mm/s³)

    // 工作空间限制
    float32 x_min_mm = 0.0f;
    float32 x_max_mm = 500.0f;
    float32 y_min_mm = 0.0f;
    float32 y_max_mm = 500.0f;
    float32 z_min_mm = 0.0f;
    float32 z_max_mm = 100.0f;

    // 审计日志
    std::string audit_log_path = "logs/audit";
    int32 audit_log_max_size_mb = 100;    // 单个日志文件最大大小(MB)
    int32 audit_log_retention_days = 90;  // 日志保留天数

    // 网络安全
    bool enable_ip_whitelist = false;       // 启用IP白名单
    std::vector<std::string> ip_whitelist;  // IP白名单

    // T073: 硬件安全连锁配置
    bool interlock_enabled = false;                 // 启用连锁监控
    int16 emergency_stop_input = 0;                 // 急停输入端口
    int16 safety_door_input = 1;                    // 安全门输入端口
    int16 pressure_sensor_input = 2;                // 气压传感器输入端口
    int16 temperature_sensor_input = 3;             // 温度传感器输入端口
    int16 voltage_sensor_input = 4;                 // 电压传感器输入端口
    int32 interlock_poll_interval_ms = 50;          // 轮询间隔(ms)
    float32 pressure_critical_low = 0.4f;           // 气压临界低值(MPa)
    float32 pressure_warning_low = 0.5f;            // 气压警告低值(MPa)
    float32 pressure_warning_high = 0.7f;           // 气压警告高值(MPa)
    float32 pressure_critical_high = 0.8f;          // 气压临界高值(MPa)
    float32 temperature_critical_low = 5.0f;        // 温度临界低值(°C)
    float32 temperature_normal_low = 10.0f;         // 温度正常低值(°C)
    float32 temperature_normal_high = 50.0f;        // 温度正常高值(°C)
    float32 temperature_critical_high = 55.0f;      // 温度临界高值(°C)
    float32 voltage_min = 220.0f;                   // 最低电压(V)
    float32 voltage_max = 240.0f;                   // 最高电压(V)
    int32 interlock_self_test_interval_hours = 24;  // 自检间隔(小时)
};

// 安全配置加载器
class SecurityConfigLoader {
   public:
    SecurityConfigLoader() = default;

    // 加载配置
    bool Load(const std::string& config_file);

    // 获取配置
    const SecurityConfig& GetConfig() const {
        return config_;
    }

    // 验证配置有效性
    bool Validate() const;

   private:
    Siligen::Application::ConfigurationVersionService version_mgr_;
    SecurityConfig config_;

    // 解析单行配置
    bool ParseLine(const std::string& section, const std::string& key, const std::string& value);

    // 解析IP白名单
    void ParseIPWhitelist(const std::string& value);
};

}  // namespace Siligen


