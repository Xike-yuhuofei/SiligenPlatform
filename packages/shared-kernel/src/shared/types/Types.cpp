#include "Types.h"

#include <sstream>
#include <utility>

namespace Siligen {

// 字符串转换函数实现
std::string ToString(DispenserState state) {
    switch (state) {
        case DispenserState::UNINITIALIZED:
            return "UNINITIALIZED";
        case DispenserState::INITIALIZING:
            return "INITIALIZING";
        case DispenserState::READY:
            return "READY";
        case DispenserState::DISPENSING:
            return "DISPENSING";
        case DispenserState::PAUSED:
            return "PAUSED";
        case DispenserState::ERROR_STATE:
            return "ERROR_STATE";
        case DispenserState::EMERGENCY_STOP:
            return "EMERGENCY_STOP";
        default:
            return "UNKNOWN";
    }
}

std::string ToString(DispensingMode mode) {
    switch (mode) {
        case DispensingMode::CONTACT:
            return "CONTACT";
        case DispensingMode::NON_CONTACT:
            return "NON_CONTACT";
        case DispensingMode::JETTING:
            return "JETTING";
        case DispensingMode::POSITION_TRIGGER:
            return "POSITION_TRIGGER";
        default:
            return "UNKNOWN";
    }
}

// Phase 3: T033-T034 - ToString(CMPTriggerMode) 已移至 shared/types/CMPTypes.h
// 使用 Siligen::Shared::Types::CMPTriggerModeToString(mode) 替代

std::string ToString(AxisStatus status) {
    static const std::pair<AxisStatus, const char*> STATUS_FLAGS[] = {{AxisStatus::ESTOP, "ESTOP"},
                                                                      {AxisStatus::SV_ALARM, "SV_ALARM"},
                                                                      {AxisStatus::POS_SOFT_LIMIT, "POS_SOFT_LIMIT"},
                                                                      {AxisStatus::NEG_SOFT_LIMIT, "NEG_SOFT_LIMIT"},
                                                                      {AxisStatus::FOLLOW_ERR, "FOLLOW_ERR"},
                                                                      {AxisStatus::POS_HARD_LIMIT, "POS_HARD_LIMIT"},
                                                                      {AxisStatus::NEG_HARD_LIMIT, "NEG_HARD_LIMIT"},
                                                                      {AxisStatus::ENABLE, "ENABLE"},
                                                                      {AxisStatus::RUNNING, "RUNNING"},
                                                                      {AxisStatus::ARRIVE, "ARRIVE"},
                                                                      {AxisStatus::HOME_SUCCESS, "HOME_SUCCESS"}};
    std::string result;
    uint32 status_val = static_cast<uint32>(status);
    for (const auto& [flag, name] : STATUS_FLAGS) {
        if ((status_val & static_cast<uint32>(flag)) != 0) {
            if (!result.empty()) {
                result += "|";
            }
            result += name;
        }
    }
    return result.empty() ? "NONE" : result;
}

std::string ToString(Axis axis) {
    switch (axis) {
        case Axis::X:
            return "X";
        case Axis::Y:
            return "Y";
        case Axis::Z:
            return "Z";
        case Axis::A:
            return "A";
        case Axis::B:
            return "B";
        case Axis::C:
            return "C";
        case Axis::U:
            return "U";
        case Axis::V:
            return "V";
        default:
            return "UNKNOWN";
    }
}

// 状态检查函数实现
bool IsError(DispenserState state) {
    return state == DispenserState::ERROR_STATE || state == DispenserState::EMERGENCY_STOP;
}

bool IsMoving(DispenserState state) {
    return state == DispenserState::DISPENSING;
}

bool CanDispense(DispenserState state) {
    return state == DispenserState::READY || state == DispenserState::DISPENSING;
}

}  // namespace Siligen