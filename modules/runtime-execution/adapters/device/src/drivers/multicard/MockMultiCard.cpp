#include "siligen/device/adapters/drivers/multicard/MockMultiCard.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>

namespace Siligen {
namespace Infrastructure {
namespace Hardware {

namespace {

constexpr auto kMockHomingDuration = std::chrono::milliseconds(120);

short NormalizeAxis(short axis) {
    return axis < 1 ? static_cast<short>(axis + 1) : axis;
}

bool IsAxisMasked(long mask, short axis) {
    if (mask == 0) {
        return true;
    }
    if (axis <= 0) {
        return false;
    }
    return (mask & (1L << (axis - 1))) != 0;
}

}  // namespace

MockMultiCard::MockMultiCard() {
    // Initialize with empty state
}

MockMultiCard::~MockMultiCard() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    card_states_.clear();
}

MockMultiCard::AxisState& MockMultiCard::GetOrCreateAxisStateUnlocked(short axis) {
    return axes_[NormalizeAxis(axis)];
}

void MockMultiCard::StopAxisUnlocked(short axis) {
    AxisState& state = GetOrCreateAxisStateUnlocked(axis);
    state.running = false;
    state.commanded_velocity_pulse_per_ms = 0.0;
    state.profile_velocity_pulse_per_ms = 0.0;
    state.encoder_velocity_pulse_per_ms = 0.0;
    state.in_position = true;
}

long MockMultiCard::ComposeAxisStatusUnlocked(short axis) const {
    const short normalized_axis = NormalizeAxis(axis);
    long status = 0;

    const auto override_it = axis_status_.find(normalized_axis);
    if (override_it != axis_status_.end()) {
        status = override_it->second;
    }

    const auto axis_it = axes_.find(normalized_axis);
    if (axis_it == axes_.end()) {
        return status;
    }

    const AxisState& state = axis_it->second;
    status &= ~(AXIS_STATUS_ENABLE |
                AXIS_STATUS_RUNNING |
                AXIS_STATUS_HOME_RUNNING |
                AXIS_STATUS_HOME_SUCESS |
                AXIS_STATUS_HOME_FAIL |
                AXIS_STATUS_SV_ALARM |
                AXIS_STATUS_FOLLOW_ERR |
                AXIS_STATUS_ESTOP |
                AXIS_STATUS_ARRIVE);

    if (state.enabled) {
        status |= AXIS_STATUS_ENABLE;
    }
    if (state.running) {
        status |= AXIS_STATUS_RUNNING;
    }
    if (state.homing) {
        status |= AXIS_STATUS_HOME_RUNNING;
    }
    if (state.homed) {
        status |= AXIS_STATUS_HOME_SUCESS;
    }
    if (state.home_failed) {
        status |= AXIS_STATUS_HOME_FAIL;
    }
    if (state.servo_alarm) {
        status |= AXIS_STATUS_SV_ALARM;
    }
    if (state.following_error) {
        status |= AXIS_STATUS_FOLLOW_ERR;
    }
    if (state.estop) {
        status |= AXIS_STATUS_ESTOP;
    }
    if (state.in_position) {
        status |= AXIS_STATUS_ARRIVE;
    }

    return status;
}

void MockMultiCard::AdvanceAxisStateUnlocked(short axis, const std::chrono::steady_clock::time_point& now) {
    const short normalized_axis = NormalizeAxis(axis);
    AxisState& state = GetOrCreateAxisStateUnlocked(normalized_axis);
    if (state.last_update_time == std::chrono::steady_clock::time_point{}) {
        state.last_update_time = now;
    }

    if (state.homing &&
        state.homing_complete_time != std::chrono::steady_clock::time_point{} &&
        now >= state.homing_complete_time) {
        state.homing = false;
        state.homed = true;
        state.home_failed = false;
        state.enabled = true;
        state.home_status = 1;
        state.position = 0;
        state.commanded_velocity_pulse_per_ms = 0.0;
        state.profile_velocity_pulse_per_ms = 0.0;
        state.encoder_velocity_pulse_per_ms = 0.0;
        state.running = false;
        state.in_position = true;
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - state.last_update_time);
    if (state.running && elapsed.count() > 0) {
        const double delta = state.commanded_velocity_pulse_per_ms * static_cast<double>(elapsed.count());
        state.position += static_cast<long>(std::llround(delta));
        state.profile_velocity_pulse_per_ms = state.commanded_velocity_pulse_per_ms;
        state.encoder_velocity_pulse_per_ms = state.commanded_velocity_pulse_per_ms;
        state.in_position = false;
    }

    if (!state.running && !state.homing) {
        state.profile_velocity_pulse_per_ms = 0.0;
        state.encoder_velocity_pulse_per_ms = 0.0;
        if (!state.home_failed) {
            state.home_status = state.homed ? 1 : 0;
        }
        if (state.enabled) {
            state.in_position = true;
        }
    }

    state.last_update_time = now;
    axis_status_[normalized_axis] = ComposeAxisStatusUnlocked(normalized_axis);
}

void MockMultiCard::AdvanceAllAxesUnlocked(const std::chrono::steady_clock::time_point& now) {
    for (auto& [axis, state] : axes_) {
        (void)state;
        AdvanceAxisStateUnlocked(axis, now);
    }
}

// === Core connection API ===

int MockMultiCard::MC_Open(short nCardNum,
                           char* cPCEthernetIP,
                           unsigned short nPCEthernetPort,
                           char* cCardEthernetIP,
                           unsigned short nCardEthernetPort) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    open_call_count_++;

    if (nCardNum < 1 || nCardNum > 8) {
        return -1;
    }
    if (cPCEthernetIP == nullptr || cCardEthernetIP == nullptr) {
        return -1;
    }
    if (strlen(cPCEthernetIP) == 0 || strlen(cCardEthernetIP) == 0) {
        return -1;
    }

    auto it = card_states_.find(nCardNum);
    if (it != card_states_.end() && it->second.is_open) {
        return -1;
    }

    MockCardState state;
    state.is_open = true;
    state.is_reset = false;
    state.local_ip = cPCEthernetIP;
    state.card_ip = cCardEthernetIP;
    state.local_port = nPCEthernetPort;
    state.card_port = nCardEthernetPort;

    card_states_[nCardNum] = state;
    return 0;
}

int MockMultiCard::MC_Close(void) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    for (auto& pair : card_states_) {
        pair.second.is_open = false;
        pair.second.is_reset = false;
    }
    for (auto& [axis, state] : axes_) {
        (void)axis;
        state.enabled = false;
        state.running = false;
        state.homing = false;
        state.homed = false;
        state.home_failed = false;
        state.home_status = 0;
        state.commanded_velocity_pulse_per_ms = 0.0;
        state.profile_velocity_pulse_per_ms = 0.0;
        state.encoder_velocity_pulse_per_ms = 0.0;
        state.in_position = true;
    }
    close_call_count_++;
    return 0;
}

int MockMultiCard::MC_Reset() {
    std::lock_guard<std::mutex> lock(state_mutex_);

    reset_call_count_++;
    bool any_open = false;
    for (const auto& pair : card_states_) {
        if (pair.second.is_open) {
            any_open = true;
            break;
        }
    }
    if (!any_open) {
        return -1;
    }

    for (auto& pair : card_states_) {
        if (pair.second.is_open) {
            pair.second.is_reset = true;
        }
    }
    return 0;
}

int MockMultiCard::MC_ResetAllM() {
    std::lock_guard<std::mutex> lock(state_mutex_);

    bool any_open = false;
    for (const auto& pair : card_states_) {
        if (pair.second.is_open) {
            any_open = true;
            break;
        }
    }
    if (!any_open) {
        return -1;
    }

    for (auto& pair : card_states_) {
        if (pair.second.is_open) {
            pair.second.is_reset = true;
        }
    }
    return 0;
}

// === Axis control ===

int MockMultiCard::MC_AxisOn(short axis, short profile) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)profile;

    const auto now = std::chrono::steady_clock::now();
    AxisState& state = GetOrCreateAxisStateUnlocked(axis);
    AdvanceAxisStateUnlocked(axis, now);
    state.enabled = true;
    state.in_position = !state.running && !state.homing;
    state.last_update_time = now;
    axis_status_[NormalizeAxis(axis)] = ComposeAxisStatusUnlocked(axis);
    return 0;
}

int MockMultiCard::MC_AxisOff(short axis, short profile) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)profile;

    AxisState& state = GetOrCreateAxisStateUnlocked(axis);
    StopAxisUnlocked(axis);
    state.enabled = false;
    state.homing = false;
    state.homed = false;
    state.home_failed = false;
    state.home_status = 0;
    state.in_position = true;
    axis_status_[NormalizeAxis(axis)] = ComposeAxisStatusUnlocked(axis);
    return 0;
}

int MockMultiCard::MC_SetPos(short axis, long position) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    AxisState& state = GetOrCreateAxisStateUnlocked(axis);
    state.position = position;
    state.in_position = true;
    state.last_update_time = std::chrono::steady_clock::now();
    axis_status_[NormalizeAxis(axis)] = ComposeAxisStatusUnlocked(axis);
    return 0;
}

int MockMultiCard::MC_GetPos(short axis, long* position) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (position == nullptr) {
        return -1;
    }

    AdvanceAxisStateUnlocked(axis, std::chrono::steady_clock::now());
    *position = GetOrCreateAxisStateUnlocked(axis).position;
    return 0;
}

int MockMultiCard::MC_GetAxisStatus(short axis, unsigned long* status) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (status == nullptr) {
        return -1;
    }

    AdvanceAxisStateUnlocked(axis, std::chrono::steady_clock::now());
    *status = static_cast<unsigned long>(ComposeAxisStatusUnlocked(axis));
    return 0;
}

int MockMultiCard::MC_GetAxisEncPos(short axis, double* pos) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (pos == nullptr) {
        return -1;
    }

    AdvanceAxisStateUnlocked(axis, std::chrono::steady_clock::now());
    *pos = static_cast<double>(GetOrCreateAxisStateUnlocked(axis).position);
    return 0;
}

int MockMultiCard::MC_GetAxisPrfVel(short axis, double* vel, short count, unsigned long* clock) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)count;

    if (vel == nullptr) {
        return -1;
    }

    AdvanceAxisStateUnlocked(axis, std::chrono::steady_clock::now());
    *vel = GetOrCreateAxisStateUnlocked(axis).profile_velocity_pulse_per_ms;
    if (clock != nullptr) {
        *clock = 0;
    }
    return 0;
}

int MockMultiCard::MC_GetAxisEncVel(short axis, double* vel, short count, unsigned long* clock) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)count;

    if (vel == nullptr) {
        return -1;
    }

    AdvanceAxisStateUnlocked(axis, std::chrono::steady_clock::now());
    *vel = GetOrCreateAxisStateUnlocked(axis).encoder_velocity_pulse_per_ms;
    if (clock != nullptr) {
        *clock = 0;
    }
    return 0;
}

int MockMultiCard::MC_GetAlarmOnOff(short axis, short* alarmOnOff) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (alarmOnOff == nullptr) {
        return -1;
    }

    AdvanceAxisStateUnlocked(axis, std::chrono::steady_clock::now());
    *alarmOnOff = GetOrCreateAxisStateUnlocked(axis).servo_alarm ? 1 : 0;
    return 0;
}

int MockMultiCard::MC_HomeStart(short axis) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    const auto now = std::chrono::steady_clock::now();
    AxisState& state = GetOrCreateAxisStateUnlocked(axis);
    AdvanceAxisStateUnlocked(axis, now);
    state.enabled = true;
    state.running = false;
    state.homing = true;
    state.homed = false;
    state.home_failed = false;
    state.commanded_velocity_pulse_per_ms = 0.0;
    state.profile_velocity_pulse_per_ms = 0.0;
    state.encoder_velocity_pulse_per_ms = 0.0;
    state.home_status = 0;
    state.in_position = false;
    state.homing_complete_time = now + kMockHomingDuration;
    state.last_update_time = now;
    axis_status_[NormalizeAxis(axis)] = ComposeAxisStatusUnlocked(axis);
    return 0;
}

int MockMultiCard::MC_Stop(long mask, short mode) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)mode;

    for (auto& [crd_id, crd_sys] : coordinate_systems_) {
        (void)crd_id;
        crd_sys.is_running = false;
    }

    AdvanceAllAxesUnlocked(std::chrono::steady_clock::now());
    for (auto& [axis, state] : axes_) {
        (void)state;
        if (IsAxisMasked(mask, axis)) {
            StopAxisUnlocked(axis);
            axis_status_[NormalizeAxis(axis)] = ComposeAxisStatusUnlocked(axis);
        }
    }
    return 0;
}

int MockMultiCard::MC_StopEx(long crd_mask, long crd_option, long axis_mask, long axis_option) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)crd_option;
    (void)axis_option;

    for (auto& [crd_id, crd_sys] : coordinate_systems_) {
        if (crd_mask == 0 || (crd_mask & (1L << crd_id)) != 0) {
            crd_sys.is_running = false;
        }
    }

    AdvanceAllAxesUnlocked(std::chrono::steady_clock::now());
    for (auto& [axis, state] : axes_) {
        (void)state;
        if (IsAxisMasked(axis_mask, axis)) {
            StopAxisUnlocked(axis);
            axis_status_[NormalizeAxis(axis)] = ComposeAxisStatusUnlocked(axis);
        }
    }
    return 0;
}

// === Trap / Jog motion ===

int MockMultiCard::MC_PrfTrap(short axis) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    AxisState& state = GetOrCreateAxisStateUnlocked(axis);
    state.trap_mode = true;
    state.jog_mode = false;
    return 0;
}

int MockMultiCard::MC_SetVel(short axis, double velocity) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    AxisState& state = GetOrCreateAxisStateUnlocked(axis);
    state.commanded_velocity_pulse_per_ms = velocity;
    return 0;
}

int MockMultiCard::MC_Update(long mask) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    const auto now = std::chrono::steady_clock::now();
    AdvanceAllAxesUnlocked(now);
    for (auto& [axis, state] : axes_) {
        if (!IsAxisMasked(mask, axis) || !state.enabled) {
            continue;
        }
        if (std::abs(state.commanded_velocity_pulse_per_ms) > 0.0) {
            state.running = true;
            state.homing = false;
            state.homed = false;
            state.home_failed = false;
            state.home_status = 0;
            state.profile_velocity_pulse_per_ms = state.commanded_velocity_pulse_per_ms;
            state.encoder_velocity_pulse_per_ms = state.commanded_velocity_pulse_per_ms;
            state.in_position = false;
            state.last_update_time = now;
            axis_status_[NormalizeAxis(axis)] = ComposeAxisStatusUnlocked(axis);
        }
    }
    return 0;
}

// === Coordinate motion ===

int MockMultiCard::MC_GetCrdSpace(short crd, long* space) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (space == nullptr) {
        return -1;
    }
    *space = 4096;
    return 0;
}

int MockMultiCard::MC_CrdSpace(short crd, long* space, short fifo) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)fifo;

    if (space == nullptr) {
        return -1;
    }
    if (crd < 0 || crd > 1) {
        return -1;
    }

    CoordinateSystem& crd_sys = coordinate_systems_[crd];
    crd_sys.configured = true;
    for (int i = 0; i < 4; ++i) {
        crd_sys.axis_map[i] = space[i];
    }
    return 0;
}

int MockMultiCard::MC_CrdClear(short crd, short fifo) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)crd;
    (void)fifo;
    return 0;
}

int MockMultiCard::MC_LnXY(
    short crd, long x, long y, double synVel, double synAcc, double endVel, short fifo, long segment_id) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)synVel;
    (void)synAcc;
    (void)endVel;
    (void)fifo;
    (void)segment_id;

    auto it = coordinate_systems_.find(crd);
    if (it == coordinate_systems_.end() || !it->second.configured) {
        return -1;
    }

    it->second.trajectory_x.push_back(x);
    it->second.trajectory_y.push_back(y);
    return 0;
}

int MockMultiCard::MC_ArcXYR(short crd,
                             long x,
                             long y,
                             double radius,
                             short direction,
                             double synVel,
                             double synAcc,
                             double endVel,
                             short fifo) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)radius;
    (void)direction;
    (void)synVel;
    (void)synAcc;
    (void)endVel;
    (void)fifo;

    auto it = coordinate_systems_.find(crd);
    if (it == coordinate_systems_.end() || !it->second.configured) {
        return -1;
    }

    it->second.trajectory_x.push_back(x);
    it->second.trajectory_y.push_back(y);
    return 0;
}

int MockMultiCard::MC_ArcXYC(short crd,
                             long x,
                             long y,
                             double cx,
                             double cy,
                             short direction,
                             double synVel,
                             double synAcc,
                             double endVel,
                             short fifo,
                             long segment_id) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)cx;
    (void)cy;
    (void)direction;
    (void)synVel;
    (void)synAcc;
    (void)endVel;
    (void)fifo;
    (void)segment_id;

    auto it = coordinate_systems_.find(crd);
    if (it == coordinate_systems_.end() || !it->second.configured) {
        return -1;
    }

    it->second.trajectory_x.push_back(x);
    it->second.trajectory_y.push_back(y);
    return 0;
}

int MockMultiCard::MC_CrdData(short nCrdNum, void* pCrdData, short nFifoIndex) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)nCrdNum;
    (void)pCrdData;
    (void)nFifoIndex;
    return 0;
}

int MockMultiCard::MC_CrdStart(short crd, short mask) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)mask;

    auto it = coordinate_systems_.find(crd);
    if (it == coordinate_systems_.end() || !it->second.configured) {
        return -1;
    }

    it->second.is_running = true;
    it->second.current_segment = 0;
    return 0;
}

int MockMultiCard::MC_CmpPluse(unsigned short channel,
                               unsigned short crd,
                               long start_pos,
                               long interval,
                               unsigned short count,
                               unsigned short output_mode,
                               unsigned short output_inverse) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)crd;
    (void)output_mode;
    (void)output_inverse;

    CMPTrigger& trigger = cmp_triggers_[static_cast<short>(channel)];
    trigger.enabled = true;
    trigger.axis = 1;
    trigger.start_pos = start_pos;
    trigger.interval = interval;
    trigger.count = count;
    trigger.trigger_times.clear();
    return 0;
}

int MockMultiCard::MC_CmpRpt(
    short nCmpNum, unsigned long lIntervalTime, short nTime, short nTimeFlag, unsigned long ulRptTime) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)nTime;
    (void)nTimeFlag;

    if (nCmpNum < 0 || nCmpNum > 3) {
        return -1;
    }

    CMPTrigger& trigger = cmp_triggers_[nCmpNum];
    trigger.enabled = true;
    trigger.axis = 1;
    trigger.start_pos = 0;
    trigger.interval = static_cast<long>(lIntervalTime);
    trigger.count = static_cast<long>(ulRptTime);
    trigger.trigger_times.clear();
    return 0;
}

int MockMultiCard::MC_CmpBufStop(unsigned short channelMask) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)channelMask;
    return 0;
}

int MockMultiCard::MC_CmpBufSetChannel(short nBuf1ChannelNum, short nBuf2ChannelNum) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)nBuf1ChannelNum;
    (void)nBuf2ChannelNum;
    return 0;
}

int MockMultiCard::MC_CmpBufRpt(short nEncNum,
                                short nDir,
                                short nEncFlag,
                                long lTrigValue,
                                short nCmpNum,
                                unsigned long lIntervalTime,
                                short nTime,
                                short nTimeFlag,
                                unsigned long ulRptTime) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)nEncNum;
    (void)nDir;
    (void)nEncFlag;
    (void)lTrigValue;
    (void)nCmpNum;
    (void)lIntervalTime;
    (void)nTime;
    (void)nTimeFlag;
    (void)ulRptTime;
    return 0;
}

int MockMultiCard::MC_CmpBufData(short nCmpEncodeNum,
                                 short nPluseType,
                                 short nStartLevel,
                                 short nTime,
                                 long* pBuf1,
                                 short nBufLen1,
                                 long* pBuf2,
                                 short nBufLen2,
                                 short nAbsPosFlag,
                                 short nTimerFlag) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)nCmpEncodeNum;
    (void)nPluseType;
    (void)nStartLevel;
    (void)nTime;
    (void)pBuf1;
    (void)nBufLen1;
    (void)pBuf2;
    (void)nBufLen2;
    (void)nAbsPosFlag;
    (void)nTimerFlag;
    return 0;
}

int MockMultiCard::MC_CmpBufSts(unsigned short* pStatus,
                                unsigned short* pRemainData1,
                                unsigned short* pRemainData2,
                                unsigned short* pRemainSpace1,
                                unsigned short* pRemainSpace2) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (pStatus) {
        *pStatus = 0;
    }
    if (pRemainData1) {
        *pRemainData1 = 0;
    }
    if (pRemainData2) {
        *pRemainData2 = 0;
    }
    if (pRemainSpace1) {
        *pRemainSpace1 = 0;
    }
    if (pRemainSpace2) {
        *pRemainSpace2 = 0;
    }
    return 0;
}

int MockMultiCard::MC_GetPrfPos(short axis, double* pos) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (pos == nullptr) {
        return -1;
    }

    AdvanceAxisStateUnlocked(axis, std::chrono::steady_clock::now());
    *pos = static_cast<double>(GetOrCreateAxisStateUnlocked(axis).position);
    return 0;
}

int MockMultiCard::MC_GetSts(short axis, long* sts, short from_axis, unsigned long* clock) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)from_axis;

    if (sts == nullptr) {
        return -1;
    }

    getsts_call_count_++;
    if (forced_getsts_return_value_ != 0) {
        return forced_getsts_return_value_;
    }
    if (simulate_disconnected_) {
        return -1;
    }

    AdvanceAxisStateUnlocked(axis, std::chrono::steady_clock::now());
    *sts = ComposeAxisStatusUnlocked(axis);
    axis_status_[NormalizeAxis(axis)] = *sts;

    if (clock != nullptr) {
        *clock = 0;
    }
    return 0;
}

void MockMultiCard::MC_StartDebugLog(int level) {
    (void)level;
}

int MockMultiCard::MC_GetClock(double* pClock) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (pClock == nullptr) {
        return -1;
    }

    bool any_open = false;
    for (const auto& pair : card_states_) {
        if (pair.second.is_open) {
            any_open = true;
            break;
        }
    }
    if (!any_open) {
        return -1;
    }

    *pClock = 0.0;
    return 0;
}

bool MockMultiCard::IsCardOpen(short cardNum) const {
    std::lock_guard<std::mutex> lock(state_mutex_);

    auto it = card_states_.find(cardNum);
    if (it != card_states_.end()) {
        return it->second.is_open;
    }
    return false;
}

MockCardState MockMultiCard::GetCardState(short cardNum) const {
    std::lock_guard<std::mutex> lock(state_mutex_);

    auto it = card_states_.find(cardNum);
    if (it != card_states_.end()) {
        return it->second;
    }
    return MockCardState();
}

void MockMultiCard::SimulateDisconnect() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    simulate_disconnected_ = true;
    std::cout << "[MockMultiCard] Simulating hardware disconnect (MC_GetSts will now fail)" << std::endl;
}

void MockMultiCard::ResetGetStsCounter() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    getsts_call_count_ = 0;
    simulate_disconnected_ = false;
}

void MockMultiCard::SetGetStsReturnValue(int return_value) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    forced_getsts_return_value_ = return_value;
    std::cout << "[MockMultiCard] MC_GetSts return value set to: " << return_value << std::endl;
}

void MockMultiCard::SetAxisStatus(short axis, long status) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    const short normalized_axis = NormalizeAxis(axis);
    AxisState& state = GetOrCreateAxisStateUnlocked(normalized_axis);
    state.enabled = (status & AXIS_STATUS_ENABLE) != 0;
    state.running = (status & AXIS_STATUS_RUNNING) != 0;
    state.homing = (status & AXIS_STATUS_HOME_RUNNING) != 0;
    state.homed = (status & AXIS_STATUS_HOME_SUCESS) != 0;
    state.home_failed = (status & AXIS_STATUS_HOME_FAIL) != 0;
    state.servo_alarm = (status & AXIS_STATUS_SV_ALARM) != 0;
    state.following_error = (status & AXIS_STATUS_FOLLOW_ERR) != 0;
    state.estop = (status & AXIS_STATUS_ESTOP) != 0;
    state.in_position = (status & AXIS_STATUS_ARRIVE) != 0;
    state.home_status = state.homed ? 1 : (state.home_failed ? 2 : 0);
    if (!state.running && !state.homing) {
        state.commanded_velocity_pulse_per_ms = 0.0;
        state.profile_velocity_pulse_per_ms = 0.0;
        state.encoder_velocity_pulse_per_ms = 0.0;
    }
    axis_status_[normalized_axis] = status;
}

void MockMultiCard::SetDigitalInputRaw(short card_index, long value) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    digital_inputs_raw_[card_index] = value;
}

int MockMultiCard::MC_HomeGetSts(short axis, unsigned short* status) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (status == nullptr) {
        return -1;
    }

    AdvanceAxisStateUnlocked(axis, std::chrono::steady_clock::now());
    *status = GetOrCreateAxisStateUnlocked(axis).home_status;
    return 0;
}

int MockMultiCard::MC_HomeStop(short axis) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    AxisState& state = GetOrCreateAxisStateUnlocked(axis);
    StopAxisUnlocked(axis);
    state.homing = false;
    state.home_status = 0;
    state.in_position = true;
    axis_status_[NormalizeAxis(axis)] = ComposeAxisStatusUnlocked(axis);
    return 0;
}

int MockMultiCard::MC_PrfJog(short axis) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    AxisState& state = GetOrCreateAxisStateUnlocked(axis);
    state.jog_mode = true;
    state.trap_mode = false;
    return 0;
}

int MockMultiCard::MC_SetJogPrm(short axis, void* jogPrm) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)axis;
    (void)jogPrm;
    return 0;
}

int MockMultiCard::MC_HomeSetPrm(short axis, void* homePrm) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)axis;
    (void)homePrm;
    return 0;
}

int MockMultiCard::MC_HomeSns(unsigned short sense) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    home_sense_mask_ = sense;
    return 0;
}

int MockMultiCard::MC_GetHomeSns(unsigned short* sense) {
    if (!sense) {
        return -1;
    }
    std::lock_guard<std::mutex> lock(state_mutex_);
    *sense = home_sense_mask_;
    return 0;
}

int MockMultiCard::MC_ClrSts(short axis, short count) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)count;

    AxisState& state = GetOrCreateAxisStateUnlocked(axis);
    state.home_failed = false;
    state.servo_alarm = false;
    state.following_error = false;
    state.estop = false;
    state.home_status = state.homed ? 1 : 0;
    axis_status_[NormalizeAxis(axis)] = ComposeAxisStatusUnlocked(axis);
    return 0;
}

int MockMultiCard::MC_SetTrapPrm(short axis, void* trapPrm) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)axis;
    (void)trapPrm;
    return 0;
}

int MockMultiCard::MC_SetExtDoBit(short nCardIndex, short nBitIndex, unsigned short nValue) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)nCardIndex;
    (void)nBitIndex;
    (void)nValue;
    return 0;
}

int MockMultiCard::MC_GetExtDoValue(short nCardIndex, unsigned long* pValue) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)nCardIndex;
    if (!pValue) {
        return -1;
    }
    *pValue = 0;
    return 0;
}

int MockMultiCard::MC_GetDiRaw(short nCardIndex, long* pValue) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (!pValue) {
        return -1;
    }

    const auto it = digital_inputs_raw_.find(nCardIndex);
    *pValue = (it != digital_inputs_raw_.end()) ? it->second : 0;
    return 0;
}

int MockMultiCard::MC_CrdStatus(short nCrdNum, short* pCrdStatus, long* pSegment, short FifoIndex) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)nCrdNum;
    (void)FifoIndex;
    if (pCrdStatus) {
        *pCrdStatus = 0;
    }
    if (pSegment) {
        *pSegment = 0;
    }
    return 0;
}

int MockMultiCard::MC_GetCrdPos(short nCrdNum, double* pPos) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)nCrdNum;
    if (!pPos) {
        return -1;
    }
    *pPos = 0.0;
    return 0;
}

int MockMultiCard::MC_GetCrdVel(short nCrdNum, double* pSynVel) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)nCrdNum;
    if (!pSynVel) {
        return -1;
    }
    *pSynVel = 0.0;
    return 0;
}

int MockMultiCard::MC_SetHardLimP(short axis, short type, short cardIndex, short ioIndex) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)axis;
    (void)type;
    (void)cardIndex;
    (void)ioIndex;
    return 0;
}

int MockMultiCard::MC_SetHardLimN(short axis, short type, short cardIndex, short ioIndex) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)axis;
    (void)type;
    (void)cardIndex;
    (void)ioIndex;
    return 0;
}

int MockMultiCard::MC_LmtsOn(short axis, short limitType) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)axis;
    (void)limitType;
    return 0;
}

int MockMultiCard::MC_LmtSns(unsigned short sense) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)sense;
    return 0;
}

int MockMultiCard::MC_LmtsOff(short axis, short limitType) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)axis;
    (void)limitType;
    return 0;
}

int MockMultiCard::MC_GetLmtsOnOff(short axis, short* posLimitEnabled, short* negLimitEnabled) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)axis;
    if (posLimitEnabled) {
        *posLimitEnabled = 0;
    }
    if (negLimitEnabled) {
        *negLimitEnabled = 0;
    }
    return 0;
}

int MockMultiCard::MC_SetLmtSnsSingle(short axis, short posPolarity, short negPolarity) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)axis;
    (void)posPolarity;
    (void)negPolarity;
    return 0;
}

int MockMultiCard::MC_SetSoftLimit(short cardIndex, short axis, long posLimit, long negLimit) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)cardIndex;
    (void)axis;
    (void)posLimit;
    (void)negLimit;
    return 0;
}

int MockMultiCard::MC_EncOn(short axis) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)axis;
    return 0;
}

int MockMultiCard::MC_EncOff(short axis) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)axis;
    return 0;
}

void MockMultiCard::TickMs(double dt_ms) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    simulation_time_ms_ += dt_ms;
    AdvanceAllAxesUnlocked(std::chrono::steady_clock::now());

    for (auto& [crd_id, crd_sys] : coordinate_systems_) {
        (void)crd_id;
        if (!crd_sys.is_running || crd_sys.current_segment >= crd_sys.trajectory_x.size()) {
            continue;
        }

        long target_x = crd_sys.trajectory_x[crd_sys.current_segment];
        long target_y = crd_sys.trajectory_y[crd_sys.current_segment];

        long axis_x = crd_sys.axis_map[0];
        long axis_y = crd_sys.axis_map[1];
        long current_x = axes_[axis_x].position;
        long current_y = axes_[axis_y].position;

        double velocity_pulses_per_ms = 10.0;
        double max_delta = velocity_pulses_per_ms * dt_ms;

        double dx = static_cast<double>(target_x - current_x);
        double dy = static_cast<double>(target_y - current_y);
        double distance = std::sqrt((dx * dx) + (dy * dy));

        if (distance <= max_delta) {
            axes_[axis_x].position = target_x;
            axes_[axis_y].position = target_y;
            crd_sys.current_segment++;

            if (crd_sys.current_segment >= crd_sys.trajectory_x.size()) {
                crd_sys.is_running = false;
            }
        } else {
            double ratio = max_delta / distance;
            axes_[axis_x].position += static_cast<long>(dx * ratio);
            axes_[axis_y].position += static_cast<long>(dy * ratio);
        }
    }

    for (auto& [cmp_id, trigger] : cmp_triggers_) {
        (void)cmp_id;
        if (!trigger.enabled) {
            continue;
        }

        long current_pos = axes_[trigger.axis].position;
        if (current_pos >= trigger.start_pos) {
            long offset = current_pos - trigger.start_pos;
            long trigger_index = offset / trigger.interval;
            if (trigger_index < trigger.count &&
                trigger.trigger_times.size() <= static_cast<size_t>(trigger_index)) {
                trigger.trigger_times.push_back(static_cast<long>(simulation_time_ms_));
            }
        }
    }
}

double MockMultiCard::GetSimulationTime() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return simulation_time_ms_;
}

std::vector<long> MockMultiCard::GetCMPTriggerTimes(short cmp) const {
    std::lock_guard<std::mutex> lock(state_mutex_);

    auto it = cmp_triggers_.find(cmp);
    if (it == cmp_triggers_.end()) {
        return {};
    }
    return it->second.trigger_times;
}

int MockMultiCard::MC_SetOverride(short crd, double ratio) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (coordinate_systems_.find(crd) != coordinate_systems_.end()) {
        coordinate_systems_[crd].velocity_override = ratio;
    }
    return 0;
}

int MockMultiCard::MC_G00SetOverride(short crd, double ratio) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (coordinate_systems_.find(crd) != coordinate_systems_.end()) {
        coordinate_systems_[crd].g00_velocity_override = ratio;
    }
    return 0;
}

int MockMultiCard::MC_CrdSMoveEnable(short crd, double jerk) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (coordinate_systems_.find(crd) != coordinate_systems_.end()) {
        coordinate_systems_[crd].s_curve_enabled = true;
        coordinate_systems_[crd].jerk = jerk;
    }
    return 0;
}

int MockMultiCard::MC_CrdSMoveDisable(short crd) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (coordinate_systems_.find(crd) != coordinate_systems_.end()) {
        coordinate_systems_[crd].s_curve_enabled = false;
        coordinate_systems_[crd].jerk = 0.0;
    }
    return 0;
}

int MockMultiCard::MC_SetConstLinearVelFlag(short crd, short flag) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (coordinate_systems_.find(crd) != coordinate_systems_.end()) {
        coordinate_systems_[crd].const_linear_vel_enabled = (flag != 0);
    }
    return 0;
}

int MockMultiCard::MC_GetLookAheadSpace(short crd, long* space, short fifo) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)fifo;
    if (space == nullptr) {
        return -1;
    }
    auto it = coordinate_systems_.find(crd);
    *space = (it != coordinate_systems_.end()) ? it->second.lookahead_space : 1000;
    return 0;
}

}  // namespace Hardware
}  // namespace Infrastructure
}  // namespace Siligen
