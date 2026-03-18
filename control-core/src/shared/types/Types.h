#pragma once

// 基础类型定义 (Basic type definitions)
#include <cstdint>
#include <string>

#include "shared/errors/ErrorHandler.h"

// 类型安全的轴号定义
#include "AxisTypes.h"

// 基础类型别名
using float32 = float;
using float64 = double;
using int8 = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;
using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

// Bridge basic aliases into Shared::Types for legacy call sites.
namespace Siligen::Shared::Types {
using ::float32;
using ::float64;
using ::int8;
using ::int16;
using ::int32;
using ::int64;
using ::uint8;
using ::uint16;
using ::uint32;
using ::uint64;
}  // namespace Siligen::Shared::Types

// 注意: 不要在此包含CMPTypes.h，会导致循环依赖
// 循环依赖链: Types.h → CMPTypes.h → Point2D.h → Types.h
// 需要使用CMPTypes的代码应自行包含 #include "CMPTypes.h"

// 硬件特定类型转发（避免循环依赖）
// #include "../../hardware/MotionController.h"

// 本地网络配置（仅限core模块内部使用）
namespace Siligen {

// 网络配置常量（硬件特定）
constexpr const char* CONTROL_CARD_IP = "192.168.0.1";
constexpr const char* LOCAL_IP = "192.168.0.200";
constexpr uint16 CONTROL_CARD_PORT = 0, LOCAL_PORT = 0;  // 0=系统自动分配（推荐，避免端口冲突）
constexpr int32 CONNECTION_TIMEOUT_MS = 5000;

// 点胶机状态枚举
enum class DispenserState : int32 {
    UNINITIALIZED = 0,  // 未初始化 (Uninitialized)
    INITIALIZING = 1,   // 初始化中 (Initializing)
    READY = 2,          // 就绪 (Ready)
    DISPENSING = 3,     // 点胶中 (Dispensing)
    PAUSED = 4,         // 暂停 (Paused)
    ERROR_STATE = 5,    // 错误 (Error)
    EMERGENCY_STOP = 6  // 紧急停止 (Emergency Stop)
};

// 点胶模式枚举
enum class DispensingMode { CONTACT, NON_CONTACT, JETTING, POSITION_TRIGGER };

// 轴状态位标志(用于MultiCard SDK状态字)
enum class AxisStatus : uint32 {
    ESTOP = 0x0001,
    SV_ALARM = 0x0002,
    POS_SOFT_LIMIT = 0x0004,
    NEG_SOFT_LIMIT = 0x0008,
    FOLLOW_ERR = 0x0010,
    POS_HARD_LIMIT = 0x0020,
    NEG_HARD_LIMIT = 0x0040,
    ENABLE = 0x0080,
    RUNNING = 0x0100,
    ARRIVE = 0x0200,
    HOME_SUCCESS = 0x0400
};

// 轴枚举
enum class Axis { X, Y, Z, A, B, C, U, V };

// ToString函数声明
std::string ToString(DispenserState state);
std::string ToString(DispensingMode mode);
std::string ToString(AxisStatus status);
std::string ToString(Axis axis);

// 状态检查函数声明
bool IsError(DispenserState state);
bool IsMoving(DispenserState state);
bool CanDispense(DispenserState state);

}  // namespace Siligen
