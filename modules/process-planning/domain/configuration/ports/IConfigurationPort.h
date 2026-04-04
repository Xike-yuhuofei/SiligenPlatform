#pragma once

#include "ValveConfig.h"
#include "shared/types/ConfigTypes.h"
#include "shared/types/DiagnosticsConfig.h"
#include "shared/types/HardwareConfiguration.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"
#include "shared/types/DispensingStrategy.h"

#include <string>
#include <vector>

namespace Siligen::Domain::Configuration::Ports {

// Using declarations for shared types
using DispensingMode = Siligen::DispensingMode;  // 使用共享类型的DispensingMode
using Shared::Types::float32;
using Shared::Types::int32;
using Shared::Types::Result;
using Siligen::Domain::Configuration::Ports::ValveSupplyConfig;
using Siligen::Shared::Types::DispensingStrategy;

/**
 * @brief 无痕内衣点胶工艺推荐值（PUR热熔胶）
 * @details 基于行业最佳实践，仅供参考，不强制
 */
namespace SeamlessUnderwearDefaults {
constexpr float32 kTemperature = 135.0f;           ///< PUR胶推荐温度 (C)
constexpr float32 kTemperatureTolerance = 5.0f;    ///< 温度容差 (C)
constexpr float32 kViscosity = 6000.0f;            ///< 推荐黏度 (cP @135C)
constexpr float32 kViscosityTolerance = 15.0f;     ///< 黏度容差 (%)
constexpr float32 kMaxSpeed = 100.0f;              ///< 推荐最大速度 (mm/s)
constexpr float32 kMaxAcceleration = 500.0f;       ///< 推荐最大加速度 (mm/s2)
constexpr float32 kPositioningTolerance = 0.05f;   ///< 推荐定位精度 (mm)
}  // namespace SeamlessUnderwearDefaults

/**
 * @brief 工艺预设类型
 */
enum class DispensingPreset {
    CUSTOM = 0,           ///< 自定义
    SEAMLESS_UNDERWEAR,   ///< 无痕内衣
    ELECTRONICS,          ///< 电子元件
    AUTOMOTIVE            ///< 汽车零部件
};

/**
 * @brief 点胶参数配置
 */
struct DispensingConfig {
    DispensingMode mode = DispensingMode::CONTACT;
    DispensingStrategy strategy = DispensingStrategy::BASELINE;
    int subsegment_count = 8;
    bool dispense_only_cruise = false;
    float32 pressure = 50.0f;
    float32 flow_rate = 1.0f;
    float32 dispensing_time = 0.1f;
    float32 wait_time = 0.05f;
    int32 supply_stabilization_ms = 500;  // 供胶阀稳压等待(ms)
    float32 retract_height = 1.0f;
    float32 approach_height = 2.0f;
    float32 open_comp_ms = 0.0f;
    float32 close_comp_ms = 0.0f;
    bool retract_enabled = false;
    float32 corner_pulse_scale = 1.0f;
    float32 curvature_speed_factor = 0.8f;
    float32 start_speed_factor = 0.5f;
    float32 end_speed_factor = 0.5f;
    float32 corner_speed_factor = 0.6f;
    float32 rapid_speed_factor = 1.0f;
    float32 trajectory_sample_dt = 0.01f;
    float32 trajectory_sample_ds = 0.0f;
    // 工艺控制参数（0/false 表示不启用）
    float32 temperature_target_c = 0.0f;
    float32 temperature_tolerance_c = 0.0f;
    float32 viscosity_target_cps = 0.0f;
    float32 viscosity_tolerance_pct = 0.0f;
    bool air_dry_required = false;
    bool air_oil_free_required = false;
    bool suck_back_enabled = false;
    float32 suck_back_ms = 0.0f;
    bool vision_feedback_enabled = false;
    // 点径目标 (mm)，胶点直径=1mm（配置项）
    float32 dot_diameter_target_mm = 0.0f;
    // 点与点边缘间隔 (mm)
    float32 dot_edge_gap_mm = 0.0f;
    float32 dot_diameter_tolerance_mm = 0.0f;
    float32 syringe_level_min_pct = 0.0f;
};

/**
 * @brief DXF 预处理配置（Python ezdxf -> .pb）
 */
struct DxfPreprocessConfig {
    bool normalize_units = true;     ///< 是否按DXF单位归一化为mm
    bool strict_r12 = false;         ///< 是否严格限制输入为R12及核心实体
    bool approx_splines = false;     ///< 是否将SPLINE近似为折线
    bool snap_enabled = false;       ///< 是否启用坐标吸附
    bool densify_enabled = false;    ///< 是否启用线段密化
    bool min_seg_enabled = false;    ///< 是否启用最小段长裁剪
    int32 spline_samples = 64;       ///< SPLINE采样点数（geomdl）
    float32 spline_max_step = 0.0f;  ///< SPLINE最大步长(mm, 0=自动)
    float32 chordal = 0.005f;        ///< 弦高容差(mm)
    float32 max_seg = 0.0f;          ///< 最大段长(mm, 0=不限制)
    float32 snap = 0.0f;             ///< 点吸附阈值(mm)
    float32 angular = 0.0f;          ///< 角度阈值(弧度, 0=不限制)
    float32 min_seg = 0.0f;          ///< 最小段长(mm)
};

/**
 * @brief DXF 轨迹规划配置（Python trajectory tool）
 */
struct DxfTrajectoryConfig {
    std::string python = "python";            ///< Python解释器
    std::string script = "scripts/engineering-data/path_to_trajectory.py";  ///< 脚本路径
};

struct DxfConfig {
    float32 offset_x = 0.0f;
    float32 offset_y = 0.0f;
};

/**
 * @brief 机器参数配置
 */
struct MachineConfig {
    float32 work_area_width = 480.0f;
    float32 work_area_height = 480.0f;
    float32 z_axis_range = 100.0f;
    float32 max_speed = 0.0f;
    float32 max_acceleration = 0.0f;
    float32 positioning_tolerance = 0.1f;
    float32 pulse_per_mm = 10000.0f;

    struct SoftLimits {
        float32 x_min = 0.0f;
        float32 x_max = 480.0f;
        float32 y_min = 0.0f;
        float32 y_max = 480.0f;
        float32 z_min = -50.0f;
        float32 z_max = 50.0f;
    } soft_limits;
};

/**
 * @brief 回零配置（对应machine_config.ini中的[Homing_AxisN]段）
 */
struct HomingConfig {
    int32 axis = 0;                       ///< 轴号（0-1）
    int32 mode = 1;                       ///< 回零模式: 0=限位回零, 1=HOME信号回零, 2=HOME+Z相信号回零, 3=索引回零
    int32 direction = 0;                  ///< 回零方向: 0=负向, 1=正向

    // 回零输入参数
    int32 home_input_source = 3;          ///< HOME输入源(MC_GetDiRaw类型,默认MC_HOME=3)
    int32 home_input_bit = -1;            ///< HOME输入位(0-15,-1表示使用轴号)
    bool home_active_low = false;         ///< HOME输入低电平有效
    int32 home_debounce_ms = 0;           ///< HOME输入去抖时间(ms,0=不去抖)

    // 速度参数 (mm/s)
    float32 ready_zero_speed_mm_s = 0.0f; ///< 回零/回零后二阶段归零统一速度(0=兼容回退到locate_velocity)
    float32 rapid_velocity = 0.0f;        ///< 快移速度
    float32 locate_velocity = 0.0f;       ///< 定位速度
    float32 index_velocity = 0.0f;        ///< INDEX速度

    // 加速度参数 (mm/s²)
    float32 acceleration = 0.0f;          ///< 加速度
    float32 deceleration = 0.0f;          ///< 减速度
    float32 jerk = 0.0f;                  ///< 加加速度

    // 距离参数 (mm)
    float32 offset = 0.0f;                ///< 偏移距离
    float32 search_distance = 520.0f;     ///< 搜索距离
    float32 escape_distance = 5.0f;       ///< 回退距离
    float32 escape_velocity = 0.0f;       ///< 回退速度(0=跟随定位速度)
    bool home_backoff_enabled = true;     ///< 启用板卡原生HOME触发后二次回退

    // 时间参数 (ms)
    int32 timeout_ms = 80000;             ///< 超时时间
    int32 settle_time_ms = 500;           ///< 稳定时间
    int32 escape_timeout_ms = 0;          ///< 回退超时(0=跟随回零超时)

    // 其他参数
    int32 retry_count = 0;                ///< 重试次数
    bool enable_escape = true;            ///< 启用回退
    bool enable_limit_switch = true;      ///< 启用限位开关
    bool enable_index = false;            ///< 启用索引
    int32 escape_max_attempts = 3;        ///< 回退尝试次数(1-3)
};

/**
 * @brief 系统配置
 */
struct SystemConfig {
    DispensingConfig dispensing;
    MachineConfig machine;
    DxfConfig dxf;
    std::vector<HomingConfig> homing_configs;
};

/**
 * @brief 配置管理端口接口
 * 定义配置的读取和保存操作
 */
class IConfigurationPort {
   public:
    virtual ~IConfigurationPort() = default;

    // ========== 配置加载和保存 ==========

    /**
     * @brief 加载系统配置
     * @details 从默认配置文件加载完整系统配置
     * @return Result<SystemConfig> 成功返回系统配置，失败返回错误信息
     *         - ErrorCode::FILE_NOT_FOUND 配置文件不存在
     *         - ErrorCode::PARSE_ERROR 配置文件格式错误
     */
    virtual Result<SystemConfig> LoadConfiguration() = 0;

    /**
     * @brief 保存系统配置
     * @details 将系统配置保存到默认配置文件
     * @param config 系统配置对象
     * @return Result<void> 成功返回Success，失败返回错误信息
     *         - ErrorCode::IO_ERROR 文件写入失败
     */
    virtual Result<void> SaveConfiguration(const SystemConfig& config) = 0;

    /**
     * @brief 重新加载配置
     * @details 从配置文件重新加载配置并更新系统
     * @return Result<void> 成功返回Success，失败返回错误信息
     *         - ErrorCode::FILE_NOT_FOUND 配置文件不存在
     *         - ErrorCode::PARSE_ERROR 配置文件格式错误
     */
    virtual Result<void> ReloadConfiguration() = 0;

    // ========== 点胶配置 ==========

    /**
     * @brief 获取点胶配置
     * @details 获取当前点胶参数配置
     * @return Result<DispensingConfig> 成功返回点胶配置，失败返回错误信息
     */
    virtual Result<DispensingConfig> GetDispensingConfig() const = 0;

    /**
     * @brief 设置点胶配置
     * @details 更新点胶参数配置
     * @param config 点胶配置对象
     * @return Result<void> 成功返回Success，失败返回错误信息
     *         - ErrorCode::INVALID_PARAMETER 参数超出有效范围
     */
    virtual Result<void> SetDispensingConfig(const DispensingConfig& config) = 0;

    // DXF 预处理配置
    virtual Result<DxfPreprocessConfig> GetDxfPreprocessConfig() const = 0;

    // DXF 轨迹规划配置
    virtual Result<DxfTrajectoryConfig> GetDxfTrajectoryConfig() const = 0;

    // ========== 诊断配置 ==========

    /**
     * @brief 获取诊断配置
     * @details 获取当前诊断开关与采样参数
     * @return Result<DiagnosticsConfig> 成功返回诊断配置，失败返回错误信息
     */
    virtual Result<Shared::Types::DiagnosticsConfig> GetDiagnosticsConfig() const = 0;

    // ========== 机器配置 ==========

    /**
     * @brief 获取机器配置
     * @details 获取当前机器参数配置
     * @return Result<MachineConfig> 成功返回机器配置，失败返回错误信息
     */
    virtual Result<MachineConfig> GetMachineConfig() const = 0;

    /**
     * @brief 设置机器配置
     * @details 更新机器参数配置
     * @param config 机器配置对象
     * @return Result<void> 成功返回Success，失败返回错误信息
     *         - ErrorCode::INVALID_PARAMETER 参数超出有效范围
     */
    virtual Result<void> SetMachineConfig(const MachineConfig& config) = 0;

    // ========== 回零配置 ==========

    /**
     * @brief 获取回零配置
     * @details 获取指定轴的回零参数配置
     * @param axis 轴号
     * @return Result<HomingConfig> 成功返回回零配置，失败返回错误信息
     *         - ErrorCode::INVALID_PARAMETER 轴号超出范围
     */
    virtual Result<HomingConfig> GetHomingConfig(int axis) const = 0;

    /**
     * @brief 设置回零配置
     * @details 更新指定轴的回零参数配置
     * @param axis 轴号
     * @param config 回零配置对象
     * @return Result<void> 成功返回Success，失败返回错误信息
     *         - ErrorCode::INVALID_PARAMETER 轴号或参数超出范围
     */
    virtual Result<void> SetHomingConfig(int axis, const HomingConfig& config) = 0;

    /**
     * @brief 获取所有回零配置
     * @details 获取所有轴的回零参数配置
     * @return Result<std::vector<HomingConfig>> 成功返回回零配置列表，失败返回错误信息
     */
    virtual Result<std::vector<HomingConfig>> GetAllHomingConfigs() const = 0;

    /**
     * @brief 获取供胶阀配置
     * @details 从配置文件读取供胶阀数字输出配置
     * @return Result<ValveSupplyConfig> 成功返回供胶阀配置，失败返回错误信息
     *         - ErrorCode::INVALID_DATA 配置数据无效
     */
    virtual Result<ValveSupplyConfig> GetValveSupplyConfig() const = 0;

    /**
     * @brief 获取点胶阀配置
     * @details 从配置文件读取点胶阀CMP触发配置
     * @return Result<DispenserValveConfig> 成功返回点胶阀配置，失败返回错误信息
     *         - ErrorCode::INVALID_DATA 配置数据无效
     */
    virtual Result<Shared::Types::DispenserValveConfig> GetDispenserValveConfig() const = 0;
    /**
     * @brief 获取阀门协调配置
     * @details 从配置文件读取阀门协调/时序参数（用于规划触发点与阀门联动）
     * @return Result<ValveCoordinationConfig> 成功返回阀门协调配置，失败返回错误信息
     *         - ErrorCode::INVALID_DATA 配置数据无效
     */
    virtual Result<Shared::Types::ValveCoordinationConfig> GetValveCoordinationConfig() const = 0;

    /**
     * @brief 获取速度采样配置
     * @details 从配置文件读取DXF执行速度采样参数
     * @return Result<VelocityTraceConfig> 成功返回速度采样配置，失败返回错误信息
     *         - ErrorCode::INVALID_DATA 配置数据无效
     */
    virtual Result<Shared::Types::VelocityTraceConfig> GetVelocityTraceConfig() const = 0;

    // ========== 配置验证 ==========

    /**
     * @brief 验证配置
     * @details 检查当前配置是否有效
     * @return Result<bool> 成功返回验证结果
     *         - true 配置有效
     *         - false 配置存在错误
     */
    virtual Result<bool> ValidateConfiguration() const = 0;

    /**
     * @brief 获取验证错误信息
     * @details 获取配置验证发现的所有错误信息
     * @return Result<std::vector<std::string>> 成功返回错误信息列表
     */
    virtual Result<std::vector<std::string>> GetValidationErrors() const = 0;

    // ========== 配置备份和恢复 ==========

    /**
     * @brief 备份配置
     * @details 将当前配置备份到指定路径
     * @param backup_path 备份文件路径
     * @return Result<void> 成功返回Success，失败返回错误信息
     *         - ErrorCode::IO_ERROR 文件写入失败
     */
    virtual Result<void> BackupConfiguration(const std::string& backup_path) = 0;

    /**
     * @brief 恢复配置
     * @details 从备份文件恢复配置
     * @param backup_path 备份文件路径
     * @return Result<void> 成功返回Success，失败返回错误信息
     *         - ErrorCode::FILE_NOT_FOUND 备份文件不存在
     *         - ErrorCode::PARSE_ERROR 备份文件格式错误
     */
    virtual Result<void> RestoreConfiguration(const std::string& backup_path) = 0;

    // ========== 硬件模式配置 ==========

    /**
     * @brief 获取硬件模式
     * @details 获取当前硬件工作模式
     * @return Result<HardwareMode> 成功返回硬件模式枚举值，失败返回错误信息
     */
    virtual Result<Shared::Types::HardwareMode> GetHardwareMode() const = 0;

    /**
     * @brief 获取硬件配置
     * @details 获取硬件连接配置参数
     * @return Result<HardwareConfiguration> 成功返回硬件配置，失败返回错误信息
     */
    virtual Result<Shared::Types::HardwareConfiguration> GetHardwareConfiguration() const = 0;
};

}  // namespace Siligen::Domain::Configuration::Ports

