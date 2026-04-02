#pragma once

#include "shared/types/AxisTypes.h"
#include "shared/types/DispensingStrategy.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Siligen {
namespace Domain {
namespace Configuration {
namespace ValueObjects {

using Siligen::Shared::Types::DispensingStrategy;

/**
 * @brief 三维位置
 */
struct Position3D {
    double x;
    double y;
    double z;

    Position3D() : x(0.0), y(0.0), z(0.0) {}

    Position3D(double xVal, double yVal, double zVal) : x(xVal), y(yVal), z(zVal) {}
};

/**
 * @brief 轴软限位
 */
struct AxisSoftLimits {
    double min;
    double max;
    bool enabled;

    AxisSoftLimits() : min(0.0), max(0.0), enabled(false) {}
};

/**
 * @brief 安全限制
 */
struct SafetyLimits {
    double maxVelocity;  // 最大速度
    double maxAcceleration;
    double maxJerk;
    std::map<std::string, AxisSoftLimits> softLimits;
};

/**
 * @brief 单轴配置
 */
struct AxisConfiguration {
    Siligen::Shared::Types::LogicalAxisId axisId;
    std::string name;
    bool enabled;
    double stepsPerUnit;     // 每毫米脉冲数
    double maxVelocity;      // 最大速度
    double maxAcceleration;  // 最大加速度 (mm/s²)
    AxisSoftLimits softLimits;

    AxisConfiguration()
        : axisId(Siligen::Shared::Types::LogicalAxisId::X),
          enabled(false),
          stepsPerUnit(0.0),
          maxVelocity(0.0),
          maxAcceleration(0.0) {}
};

/**
 * @brief 旋转坐标 (3D)
 */
struct Rotation3D {
    double x;
    double y;
    double z;

    Rotation3D() : x(0.0), y(0.0), z(0.0) {}
};

/**
 * @brief 坐标系配置
 */
struct CoordinateSystemConfig {
    std::string units;  // 'mm' | 'inch'
    Position3D origin;
    Rotation3D rotation;

    CoordinateSystemConfig() : units("mm") {}
};

/**
 * @brief 运动配置
 */
struct MotionConfiguration {
    double defaultVelocity;      // 默认速度
    double defaultAcceleration;  // 默认加速度 (mm/s²)
    double defaultDeceleration;  // 默认减速度 (mm/s²)
    double maxVelocity;          // 最大速度
    double maxAcceleration;
    double maxDeceleration;
    double jerk;
    std::vector<AxisConfiguration> axes;
    CoordinateSystemConfig coordinateSystem;

    MotionConfiguration()
        : defaultVelocity(0.0),
          defaultAcceleration(0.0),
          defaultDeceleration(0.0),
          maxVelocity(0.0),
          maxAcceleration(0.0),
          maxDeceleration(0.0),
          jerk(0.0) {}
};

/**
 * @brief 急停配置
 */
struct EmergencyStopConfig {
    bool enabled;
    std::string behavior;   // 'immediate_stop' | 'controlled_stop'
    std::string resetMode;  // 'manual' | 'automatic'

    EmergencyStopConfig() : enabled(true), behavior("immediate_stop"), resetMode("manual") {}
};

/**
 * @brief 联锁配置
 */
struct InterlockConfig {
    bool enabled;
    bool check_emergency_stop;
    bool check_servo_alarm;
    bool check_safety_door;
    bool check_pressure;
    bool check_temperature;
    bool check_voltage;

    InterlockConfig()
        : enabled(true),
          check_emergency_stop(true),
          check_servo_alarm(true),
          check_safety_door(true),
          check_pressure(true),
          check_temperature(true),
          check_voltage(true) {}
};

/**
 * @brief 警告配置
 */
struct WarningConfig {
    double positionDeviation;
    double velocityDeviation;
    double temperatureLimit;
    double vibrationLimit;

    WarningConfig() : positionDeviation(0.0), velocityDeviation(0.0), temperatureLimit(0.0), vibrationLimit(0.0) {}
};

/**
 * @brief 安全配置
 */
struct SafetyConfiguration {
    EmergencyStopConfig emergencyStop;
    SafetyLimits limits;
    InterlockConfig interlocks;
    WarningConfig warnings;
};

/**
 * @brief 图表设置
 */
struct ChartSettings {
    double timeWindow;
    double sampleRate;
    bool autoScale;
    bool showGrid;
    bool showLegend;

    ChartSettings() : timeWindow(10.0), sampleRate(100.0), autoScale(true), showGrid(true), showLegend(true) {}
};

/**
 * @brief 显示配置
 */
struct DisplayConfiguration {
    std::string theme;  // 'light' | 'dark'
    std::string language;
    std::string units;  // 'mm' | 'inch'
    int decimalPlaces;
    int refreshRate;
    ChartSettings chartSettings;

    DisplayConfiguration() : theme("light"), language("en"), units("mm"), decimalPlaces(3), refreshRate(30) {}
};

/**
 * @brief 控制器配置
 */
struct ControllerConfig {
    std::string type;  // 'multicard' | 'other'
    std::string ipAddress;
    int port;
    int timeout;
    int retryCount;

    ControllerConfig() : type("multicard"), port(502), timeout(5000), retryCount(3) {}
};

/**
 * @brief 数字输入配置
 */
struct DigitalInputConfig {
    int port;
    int bit;
    std::string name;
    bool enabled;
    bool invert;
    int debounce;

    DigitalInputConfig() : port(0), bit(0), enabled(false), invert(false), debounce(0) {}
};

/**
 * @brief 数字输出配置
 */
struct DigitalOutputConfig {
    int port;
    int bit;
    std::string name;
    bool enabled;
    bool defaultValue;
    bool safetyValue;

    DigitalOutputConfig() : port(0), bit(0), enabled(false), defaultValue(false), safetyValue(false) {}
};

/**
 * @brief 模拟范围
 */
struct AnalogRange {
    double min;
    double max;
    std::string unit;

    AnalogRange() : min(0.0), max(10.0), unit("V") {}
};

/**
 * @brief 模拟滤波
 */
struct AnalogFilter {
    std::string type;  // 'none' | 'average' | 'median'
    int windowSize;

    AnalogFilter() : type("none"), windowSize(1) {}
};

/**
 * @brief 模拟输入配置
 */
struct AnalogInputConfig {
    int channel;
    std::string name;
    bool enabled;
    AnalogRange range;
    AnalogFilter filter;

    AnalogInputConfig() : channel(0), enabled(false) {}
};

/**
 * @brief 模拟输出配置
 */
struct AnalogOutputConfig {
    int channel;
    std::string name;
    bool enabled;
    AnalogRange range;
    double defaultValue;
    double safetyValue;

    AnalogOutputConfig() : channel(0), enabled(false), defaultValue(0.0), safetyValue(0.0) {}
};

/**
 * @brief IO配置
 */
struct IOConfiguration {
    std::vector<DigitalInputConfig> digitalInputs;
    std::vector<DigitalOutputConfig> digitalOutputs;
    std::vector<AnalogInputConfig> analogInputs;
    std::vector<AnalogOutputConfig> analogOutputs;
};

/**
 * @brief 传感器限制
 */
struct SensorLimits {
    double min;
    double max;
    double warning;
    double alarm;

    SensorLimits() : min(0.0), max(100.0), warning(80.0), alarm(90.0) {}
};

/**
 * @brief 温度传感器配置
 */
struct TemperatureSensorConfig {
    std::string id;
    std::string name;
    bool enabled;
    std::string unit;  // 'celsius' | 'fahrenheit'
    SensorLimits limits;

    TemperatureSensorConfig() : enabled(false), unit("celsius") {}
};

/**
 * @brief 压力传感器配置
 */
struct PressureSensorConfig {
    std::string id;
    std::string name;
    bool enabled;
    std::string unit;  // 'bar' | 'psi' | 'pa'
    SensorLimits limits;

    PressureSensorConfig() : enabled(false), unit("bar") {}
};

/**
 * @brief 位置传感器配置
 */
struct PositionSensorConfig {
    std::string id;
    std::string name;
    bool enabled;
    std::string type;   // 'encoder' | 'resolver' | 'potentiometer'
    double resolution;  // 位置传感器分辨率
    SensorLimits limits;

    PositionSensorConfig() : enabled(false), type("encoder"), resolution(0.001) {}
};

/**
 * @brief 传感器配置
 */
struct SensorConfiguration {
    std::vector<TemperatureSensorConfig> temperature;
    std::vector<PressureSensorConfig> pressure;
    std::vector<PositionSensorConfig> position;
};

/**
 * @brief 硬件配置
 */
struct HardwareConfiguration {
    ControllerConfig controller;
    IOConfiguration io;
    SensorConfiguration sensors;
};

/**
 * @brief 点胶器配置
 */
struct DispenserConfig {
    std::string type;  // 'pneumatic' | 'servo'
    double maxPressure;
    double minPressure;
    double resolution;  // 点胶器分辨率

    DispenserConfig() : type("pneumatic"), maxPressure(6.0), minPressure(0.1), resolution(0.01) {}
};

/**
 * @brief 阀门配置
 */
struct ValveConfig {
    std::string type;  // 'needle' | 'diaphragm' | 'ball'
    double diameter;
    std::string material;
    double maxTemperature;

    ValveConfig() : type("needle"), diameter(0.5), material("stainless_steel"), maxTemperature(80.0) {}
};

/**
 * @brief 材料配置
 */
struct MaterialConfig {
    std::string name;
    double viscosity;
    double density;
    double temperature;
    double potLife;

    MaterialConfig() : viscosity(0.0), density(0.0), temperature(25.0), potLife(0.0) {}
};

/**
 * @brief 点胶配置
 */
struct DispensingProcessConfig {
    DispensingStrategy strategy = DispensingStrategy::BASELINE;
    int subsegment_count = 8;
    bool dispense_only_cruise = false;
    double open_comp_ms = 0.0;
    double close_comp_ms = 0.0;
    bool retract_enabled = false;
    double corner_pulse_scale = 1.0;
    double curvature_speed_factor = 0.8;
    double start_speed_factor = 0.5;
    double end_speed_factor = 0.5;
    double corner_speed_factor = 0.6;
    double rapid_speed_factor = 1.0;
    double trajectory_sample_dt = 0.01;
    double trajectory_sample_ds = 0.0;
};

struct DispensingConfiguration {
    DispenserConfig dispenser;
    ValveConfig valve;
    MaterialConfig material;
    DispensingProcessConfig process;
};

/**
 * @brief 系统配置
 */
struct SystemConfiguration {
    std::string version;
    std::int64_t lastModified;
    MotionConfiguration motion;
    SafetyConfiguration safety;
    DisplayConfiguration display;
    HardwareConfiguration hardware;
    std::optional<DispensingConfiguration> dispensing;

    SystemConfiguration() : version("1.0.0"), lastModified(0) {}
};

/**
 * @brief 运动参数（用于更新）
 */
struct MotionParameters {
    std::optional<double> defaultVelocity;
    std::optional<double> defaultAcceleration;
    std::optional<double> defaultDeceleration;
    std::optional<double> maxVelocity;
    std::optional<double> maxAcceleration;
    std::optional<double> maxDeceleration;
    std::optional<double> jerk;
};

/**
 * @brief 显示设置（用于更新）
 */
struct DisplaySettings {
    std::optional<std::string> theme;  // 'light' | 'dark'
    std::optional<std::string> language;
    std::optional<std::string> units;  // 'mm' | 'inch'
    std::optional<int> decimalPlaces;
    std::optional<int> refreshRate;
};

/**
 * @brief 硬件配置（用于更新）
 */
struct HardwareConfigurationUpdate {
    std::optional<ControllerConfig> controller;
    std::optional<IOConfiguration> io;
    std::optional<SensorConfiguration> sensors;
};

/**
 * @brief 验证错误
 */
struct ValidationError {
    std::string field;
    std::string message;
    std::string code;
    std::optional<std::string> value;
};

/**
 * @brief 验证警告
 */
struct ValidationWarning {
    std::string field;
    std::string message;
    std::string code;
    std::optional<std::string> value;
};

/**
 * @brief 验证结果
 */
struct ValidationResult {
    bool isValid;
    std::vector<ValidationError> errors;
    std::vector<ValidationWarning> warnings;

    ValidationResult() : isValid(true) {}
};

/**
 * @brief 配置变更
 */
struct ConfigurationChange {
    std::string field;
    std::string oldValue;
    std::string newValue;
    std::string changeType;  // 'added' | 'modified' | 'removed'
};

/**
 * @brief 配置历史
 */
struct ConfigurationHistory {
    std::string version;
    std::int64_t timestamp;
    std::string author;
    std::string description;
    std::vector<ConfigurationChange> changes;

    ConfigurationHistory() : timestamp(0) {}
};

/**
 * @brief 配置差异
 */
struct ConfigurationDiff {
    std::string version1;
    std::string version2;
    std::vector<ConfigurationChange> additions;
    std::vector<ConfigurationChange> modifications;
    std::vector<ConfigurationChange> deletions;
};

}  // namespace ValueObjects
}  // namespace Configuration
}  // namespace Domain
}  // namespace Siligen
