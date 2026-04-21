#pragma once

#include "../types/Error.h"
#include "../types/Types.h"
#include "ErrorHandler.h"

#include <string>

namespace Siligen {
namespace Shared {

inline std::string GetSystemErrorDescription(Types::ErrorCode code) {
    switch (code) {
        case Types::ErrorCode::SUCCESS:
            return "操作成功";
        case Types::ErrorCode::UNKNOWN_ERROR:
            return "未知错误";
        case Types::ErrorCode::HARDWARE_CONNECTION_FAILED:
            return "控制卡连接失败";
        case Types::ErrorCode::MOTION_TIMEOUT:
            return "网络连接超时";
        case Types::ErrorCode::INVALID_PARAMETER:
            return "无效的参数";
        case Types::ErrorCode::CONFIGURATION_ERROR:
            return "配置错误";
        case Types::ErrorCode::EMERGENCY_STOP_ACTIVATED:
            return "急停被激活";
        case Types::ErrorCode::POSITION_OUT_OF_RANGE:
            return "位置超出范围";
        case Types::ErrorCode::VELOCITY_LIMIT_EXCEEDED:
            return "速度超出限制";
        case Types::ErrorCode::CONNECTION_FAILED:
            return "连接失败";
        case Types::ErrorCode::AXIS_NOT_HOMED:
            return "轴未回零";
        case Types::ErrorCode::HARDWARE_NOT_CONNECTED:
            return "硬件未连接";
        case Types::ErrorCode::CONFIG_FILE_NOT_FOUND:
            return "配置文件未找到";
        case Types::ErrorCode::INVALID_CONFIG_VALUE:
            return "无效的配置值";
        case Types::ErrorCode::CONFIG_PARSE_ERROR:
            return "配置解析错误";
        case Types::ErrorCode::CMP_CONFIGURATION_INVALID:
            return "CMP配置无效";
        case Types::ErrorCode::CMP_TRIGGER_SETUP_FAILED:
            return "CMP触发设置失败";
        case Types::ErrorCode::CMP_POSITION_OUT_OF_RANGE:
            return "CMP位置超出范围";
        case Types::ErrorCode::COMMAND_FAILED:
            return "命令执行失败";
        case Types::ErrorCode::INVALID_STATE:
            return "无效状态";

        // 网络错误 (Network errors)
        case Types::ErrorCode::NETWORK_CONNECTION_FAILED:
            return "网络连接失败";
        case Types::ErrorCode::NETWORK_TIMEOUT:
            return "网络超时";
        case Types::ErrorCode::NETWORK_ERROR:
            return "网络错误";

        // 硬件错误 (Hardware errors)
        case Types::ErrorCode::HARDWARE_TIMEOUT:
            return "硬件响应超时";
        case Types::ErrorCode::HARDWARE_NOT_RESPONDING:
            return "硬件无响应";
        case Types::ErrorCode::HARDWARE_COMMAND_FAILED:
            return "硬件命令执行失败";
        case Types::ErrorCode::DISPENSER_TRIGGER_INCOMPLETE:
            return "路径触发点胶计数未完整收口";

        // 连接相关错误 (Connection errors)
        case Types::ErrorCode::PORT_NOT_INITIALIZED:
            return "连接端口未初始化";

        // 运动控制错误 (Motion control errors)
        case Types::ErrorCode::MOTION_START_FAILED:
            return "运动启动失败";
        case Types::ErrorCode::MOTION_STOP_FAILED:
            return "运动停止失败";
        case Types::ErrorCode::POSITION_QUERY_FAILED:
            return "位置查询失败";
        case Types::ErrorCode::HARDWARE_OPERATION_FAILED:
            return "硬件操作失败";

        // 数据持久化错误 (Data persistence errors)
        case Types::ErrorCode::REPOSITORY_NOT_AVAILABLE:
            return "数据仓库不可用";
        case Types::ErrorCode::DATABASE_WRITE_FAILED:
            return "数据库写入失败";
        case Types::ErrorCode::DATA_SERIALIZATION_FAILED:
            return "数据序列化失败";

        // 文件解析错误 (File parsing errors)
        case Types::ErrorCode::FILE_PARSING_FAILED:
            return "文件解析失败";
        case Types::ErrorCode::PATH_OPTIMIZATION_FAILED:
            return "路径优化失败";
        case Types::ErrorCode::FILE_VALIDATION_FAILED:
            return "文件验证失败";

        // 线程操作错误 (Thread operation errors)
        case Types::ErrorCode::THREAD_START_FAILED:
            return "线程启动失败";

        // 适配器错误 (Adapter errors)
        case Types::ErrorCode::ADAPTER_EXCEPTION:
            return "适配器异常";
        case Types::ErrorCode::ADAPTER_NOT_INITIALIZED:
            return "适配器未初始化";
        case Types::ErrorCode::ADAPTER_UNKNOWN_EXCEPTION:
            return "适配器未知异常";
        case Types::ErrorCode::NOT_IMPLEMENTED:
            return "功能未实现";
        case Types::ErrorCode::TIMEOUT:
            return "操作超时";
        case Types::ErrorCode::MOTION_ERROR:
            return "运动错误";

        default:
            return "未定义的系统错误码";
    }
}


// 新错误码系统的支持 (SystemErrorCode 和 HardwareErrorCode)
inline std::string GetSystemErrorDescription(SystemErrorCode code) {
    switch (code) {
        case SystemErrorCode::SUCCESS:
            return "操作成功";
        case SystemErrorCode::UNKNOWN_ERROR:
            return "未知错误";
        case SystemErrorCode::CARD_CONNECTION_FAILED:
            return "控制卡连接失败";
        case SystemErrorCode::NETWORK_TIMEOUT:
            return "网络连接超时";
        case SystemErrorCode::INVALID_IP_ADDRESS:
            return "无效的IP地址";
        case SystemErrorCode::PORT_ACCESS_DENIED:
            return "端口访问被拒绝";
        case SystemErrorCode::AXIS_ENABLE_FAILED:
            return "轴使能失败";
        case SystemErrorCode::COORDINATE_SYSTEM_FAILED:
            return "坐标系启动失败";
        case SystemErrorCode::SERVO_ALARM_ACTIVE:
            return "伺服报警激活";
        case SystemErrorCode::LIMIT_SWITCH_TRIGGERED:
            return "限位开关触发";
        case SystemErrorCode::MOTION_COMMAND_FAILED:
            return "运动命令执行失败";
        case SystemErrorCode::POSITION_FOLLOW_ERROR:
            return "位置跟随误差过大";
        case SystemErrorCode::VELOCITY_LIMIT_EXCEEDED:
            return "速度超出限制";
        case SystemErrorCode::ACCELERATION_LIMIT_EXCEEDED:
            return "加速度超出限制";
        case SystemErrorCode::MOTION_TIMEOUT:
            return "运动超时";
        case SystemErrorCode::CMP_SETUP_FAILED:
            return "CMP设置失败";
        case SystemErrorCode::CMP_TRIGGER_FAILED:
            return "CMP触发失败";
        case SystemErrorCode::INVALID_PULSE_WIDTH:
            return "无效的脉宽";
        case SystemErrorCode::CMP_CHANNEL_ERROR:
            return "CMP通道错误";
        case SystemErrorCode::INVALID_PARAMETERS:
            return "无效的参数";
        case SystemErrorCode::PARAMETER_OUT_OF_RANGE:
            return "参数超出范围";
        case SystemErrorCode::MISSING_REQUIRED_PARAMETER:
            return "缺少必需参数";
        case SystemErrorCode::PARAMETER_CONFLICT:
            return "参数冲突";
        case SystemErrorCode::MEMORY_ALLOCATION_FAILED:
            return "内存分配失败";
        case SystemErrorCode::THREAD_CREATION_FAILED:
            return "线程创建失败";
        case SystemErrorCode::FILE_ACCESS_FAILED:
            return "文件访问失败";
        case SystemErrorCode::CONFIGURATION_ERROR:
            return "配置错误";
        case SystemErrorCode::EMERGENCY_STOP_ACTIVATED:
            return "急停被激活";
        case SystemErrorCode::SAFETY_CHECK_FAILED:
            return "安全检查失败";
        case SystemErrorCode::HARDWARE_FAULT_DETECTED:
            return "硬件故障检测";
        case SystemErrorCode::COMMUNICATION_LOST:
            return "通信丢失";

        default:
            return "未定义的系统错误码";
    }
}

inline std::string GetHardwareErrorDescription(HardwareErrorCode code) {
    switch (code) {
        case HardwareErrorCode::COMMUNICATION_FAILED:
            return "通讯失败";
        case HardwareErrorCode::CARD_OPEN_FAIL:
            return "打开控制器失败";
        case HardwareErrorCode::EXECUTION_FAILED:
            return "执行失败";
        case HardwareErrorCode::API_NOT_SUPPORTED:
            return "版本不支持该API";
        case HardwareErrorCode::PARAMETER_ERROR:
            return "参数错误";
        case HardwareErrorCode::CONTROLLER_NO_RESPONSE:
            return "运动控制器无响应";
        case HardwareErrorCode::COM_OPEN_FAILED:
            return "串口打开失败";
        case HardwareErrorCode::SERVO_OVERCURRENT:
            return "伺服过流";
        case HardwareErrorCode::SERVO_OVERVOLTAGE:
            return "伺服过压";
        case HardwareErrorCode::SERVO_UNDERVOLTAGE:
            return "伺服欠压";
        case HardwareErrorCode::ENCODER_FAILURE:
            return "编码器故障";
        case HardwareErrorCode::MOTOR_OVERHEAT:
            return "电机过热";
        case HardwareErrorCode::SERVO_PARAMETER_ERROR:
            return "伺服参数错误";
        case HardwareErrorCode::POSITION_ERROR_EXCEEDED:
            return "位置误差超限";
        case HardwareErrorCode::FOLLOWING_ERROR_EXCEEDED:
            return "跟随误差超限";
        case HardwareErrorCode::POSITIVE_HARD_LIMIT:
            return "正限位触发";
        case HardwareErrorCode::NEGATIVE_HARD_LIMIT:
            return "负限位触发";
        case HardwareErrorCode::POSITIVE_SOFT_LIMIT:
            return "正软限位触发";
        case HardwareErrorCode::NEGATIVE_SOFT_LIMIT:
            return "负软限位触发";
        case HardwareErrorCode::LIMIT_SWITCH_FAULT:
            return "限位开关故障";
        case HardwareErrorCode::ETHERNET_COMMUNICATION_FAILED:
            return "以太网通信失败";
        case HardwareErrorCode::PROTOCOL_ERROR:
            return "协议错误";
        case HardwareErrorCode::DATA_CORRUPTION:
            return "数据损坏";
        case HardwareErrorCode::COMMUNICATION_TIMEOUT:
            return "通信超时";
        case HardwareErrorCode::DEVICE_NOT_RESPONDING:
            return "设备无响应";
        case HardwareErrorCode::INTERPOLATION_FAILED:
            return "插补失败";
        case HardwareErrorCode::PATH_PLANNING_ERROR:
            return "路径规划错误";
        case HardwareErrorCode::VELOCITY_PROFILE_ERROR:
            return "速度配置错误";
        case HardwareErrorCode::ACCELERATION_PROFILE_ERROR:
            return "加速度配置错误";
        case HardwareErrorCode::JERK_LIMIT_EXCEEDED:
            return "加加速度超限";
        case HardwareErrorCode::PATH_CONTINUITY_ERROR:
            return "路径连续性错误";
        case HardwareErrorCode::CMP_BUFFER_OVERFLOW:
            return "CMP缓冲区溢出";
        case HardwareErrorCode::CMP_TIMING_ERROR:
            return "CMP时序错误";
        case HardwareErrorCode::TRIGGER_SIGNAL_LOST:
            return "触发信号丢失";
        case HardwareErrorCode::PULSE_GENERATION_FAILED:
            return "脉冲生成失败";
        case HardwareErrorCode::TRIGGER_CONFIGURATION_ERROR:
            return "触发配置错误";

        default:
            return "未定义的硬件错误码";
    }
}

}  // namespace Shared
}  // namespace Siligen
