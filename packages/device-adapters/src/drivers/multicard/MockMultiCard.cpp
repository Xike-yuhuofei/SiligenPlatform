#include "siligen/device/adapters/drivers/multicard/MockMultiCard.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

namespace Siligen {
namespace Infrastructure {
namespace Hardware {

MockMultiCard::MockMultiCard() {
    // Initialize with empty state
}

MockMultiCard::~MockMultiCard() {
    // Cleanup (if any cards still open, close them)
    std::lock_guard<std::mutex> lock(state_mutex_);
    card_states_.clear();
}

// === Core connection API ===

int MockMultiCard::MC_Open(short nCardNum,
                           char* cPCEthernetIP,
                           unsigned short nPCEthernetPort,
                           char* cCardEthernetIP,
                           unsigned short nCardEthernetPort) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    // Increment call count for every call attempt
    open_call_count_++;

    // Validate cardNum (1-8 range, mirroring real MultiCard)
    if (nCardNum < 1 || nCardNum > 8) {
        return -1;  // Invalid card number
    }

    // Validate IP address pointers and strings
    if (cPCEthernetIP == nullptr || cCardEthernetIP == nullptr) {
        return -1;  // Invalid parameters
    }

    // Validate IP addresses are not empty strings
    if (strlen(cPCEthernetIP) == 0 || strlen(cCardEthernetIP) == 0) {
        return -1;  // Empty IP address
    }

    // Check if card already open
    auto it = card_states_.find(nCardNum);
    if (it != card_states_.end() && it->second.is_open) {
        return -1;  // Card already open
    }

    // Store connection state
    MockCardState state;
    state.is_open = true;
    state.is_reset = false;
    state.local_ip = cPCEthernetIP;
    state.card_ip = cCardEthernetIP;
    state.local_port = nPCEthernetPort;
    state.card_port = nCardEthernetPort;

    card_states_[nCardNum] = state;

    return 0;  // Success
}

int MockMultiCard::MC_Close(void) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    // Mark all cards as closed
    for (auto& pair : card_states_) {
        pair.second.is_open = false;
        pair.second.is_reset = false;
    }

    close_call_count_++;

    return 0;  // Success
}

int MockMultiCard::MC_Reset() {
    std::lock_guard<std::mutex> lock(state_mutex_);

    // Increment call count for every call attempt
    reset_call_count_++;

    // Check if any card is open
    bool any_open = false;
    for (const auto& pair : card_states_) {
        if (pair.second.is_open) {
            any_open = true;
            break;
        }
    }

    if (!any_open) {
        return -1;  // No cards open
    }

    // Mark all open cards as reset
    for (auto& pair : card_states_) {
        if (pair.second.is_open) {
            pair.second.is_reset = true;
        }
    }

    return 0;  // Success
}

int MockMultiCard::MC_ResetAllM() {
    std::lock_guard<std::mutex> lock(state_mutex_);

    // 检查是否存在已打开的卡
    bool any_open = false;
    for (const auto& pair : card_states_) {
        if (pair.second.is_open) {
            any_open = true;
            break;
        }
    }

    if (!any_open) {
        return -1;  // No cards open
    }

    // 标记所有打开的卡已复位
    for (auto& pair : card_states_) {
        if (pair.second.is_open) {
            pair.second.is_reset = true;
        }
    }

    return 0;  // Success
}

// === Axis control stubs ===

int MockMultiCard::MC_AxisOn(short axis, short profile) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    // Stub: always success
    return 0;
}

int MockMultiCard::MC_AxisOff(short axis, short profile) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    // Stub: always success
    return 0;
}

int MockMultiCard::MC_SetPos(short axis, long position) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    // Stub: always success
    return 0;
}

int MockMultiCard::MC_GetPos(short axis, long* position) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (position == nullptr) {
        return -1;  // Invalid parameter
    }

    // Return actual axis position
    *position = axes_[axis].position;

    return 0;  // Success
}

int MockMultiCard::MC_GetAxisStatus(short axis, unsigned long* status) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (status == nullptr) {
        return -1;  // Invalid parameter
    }

    // Return mock status (0 = idle for simplicity)
    *status = 0;

    return 0;  // Success
}

int MockMultiCard::MC_HomeStart(short axis) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    // Stub: always success
    return 0;
}

int MockMultiCard::MC_HomeGetSts(short axis, unsigned short* status) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (status == nullptr) {
        return -1;
    }

    *status = 0;
    return 0;
}

int MockMultiCard::MC_Stop(long mask, short mode) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    for (auto& [crd_id, crd_sys] : coordinate_systems_) {
        crd_sys.is_running = false;
    }

    return 0;
}

int MockMultiCard::MC_StopEx(long crd_mask, long crd_option, long axis_mask, long axis_option) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)crd_option;
    (void)axis_mask;
    (void)axis_option;

    for (auto& [crd_id, crd_sys] : coordinate_systems_) {
        if (crd_mask == 0 || (crd_mask & (1L << crd_id)) != 0) {
            crd_sys.is_running = false;
        }
    }

    return 0;
}

// === Trap motion stubs ===

int MockMultiCard::MC_PrfTrap(short axis) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    // Stub: always success
    return 0;
}

int MockMultiCard::MC_SetVel(short axis, double velocity) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    // Stub: always success
    return 0;
}

int MockMultiCard::MC_Update(long mask) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    // Stub: always success
    return 0;
}

// === Coordinate motion stubs ===

int MockMultiCard::MC_GetCrdSpace(short crd, long* space) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (space == nullptr) {
        return -1;  // Invalid parameter
    }

    // Return mock free space (4096 for simplicity)
    *space = 4096;

    return 0;  // Success
}

int MockMultiCard::MC_CrdSpace(short crd, long* space, short fifo) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (space == nullptr) {
        return -1;  // Invalid parameter
    }

    // Validate coordinate system number (0-1)
    if (crd < 0 || crd > 1) {
        return -1;
    }

    // Configure coordinate system
    CoordinateSystem& crd_sys = coordinate_systems_[crd];
    crd_sys.configured = true;
    for (int i = 0; i < 4; ++i) {
        crd_sys.axis_map[i] = space[i];
    }

    return 0;  // Success
}

int MockMultiCard::MC_CrdClear(short crd, short fifo) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    // Stub: always success
    return 0;
}

int MockMultiCard::MC_LnXY(
    short crd, long x, long y, double synVel, double synAcc, double endVel, short fifo, long segment_id) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    // Validate coordinate system
    auto it = coordinate_systems_.find(crd);
    if (it == coordinate_systems_.end() || !it->second.configured) {
        return -1;  // Coordinate system not configured
    }

    // Add target position to trajectory buffer
    CoordinateSystem& crd_sys = it->second;
    crd_sys.trajectory_x.push_back(x);
    crd_sys.trajectory_y.push_back(y);

    // Add empty arc segment to maintain index alignment
    CoordinateSystem::ArcSegment empty_arc;
    empty_arc.center_x = 0;
    empty_arc.center_y = 0;
    empty_arc.direction = -1;  // -1 indicates linear segment
    crd_sys.arc_segments.push_back(empty_arc);

    return 0;  // Success
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
    // Stub: always success
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

    // Validate coordinate system
    auto it = coordinate_systems_.find(crd);
    if (it == coordinate_systems_.end() || !it->second.configured) {
        return -1;  // Coordinate system not configured
    }

    // Add target position to trajectory buffer
    CoordinateSystem& crd_sys = it->second;
    crd_sys.trajectory_x.push_back(x);
    crd_sys.trajectory_y.push_back(y);

    // Store arc segment information
    CoordinateSystem::ArcSegment arc_seg;
    arc_seg.center_x = static_cast<long>(cx);
    arc_seg.center_y = static_cast<long>(cy);
    arc_seg.direction = direction;
    crd_sys.arc_segments.push_back(arc_seg);

    return 0;  // Success
}

int MockMultiCard::MC_CrdData(short nCrdNum, void* pCrdData, short nFifoIndex) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    (void)nCrdNum;
    (void)pCrdData;
    (void)nFifoIndex;
    return 0;  // Success
}

int MockMultiCard::MC_CrdStart(short crd, short mask) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    // Validate coordinate system
    auto it = coordinate_systems_.find(crd);
    if (it == coordinate_systems_.end() || !it->second.configured) {
        return -1;  // Coordinate system not configured
    }

    CoordinateSystem& crd_sys = it->second;

    // Check if trajectory is empty
    if (crd_sys.trajectory_x.empty()) {
        return -1;  // No trajectory buffered
    }

    // Start coordinate system motion
    crd_sys.is_running = true;
    crd_sys.current_segment = 0;

    return 0;  // Success
}

int MockMultiCard::MC_CmpPluse(unsigned short channel,
                               unsigned short crd,
                               long start_pos,
                               long interval,
                               unsigned short count,
                               unsigned short output_mode,
                               unsigned short output_inverse) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    // Stub: always success
    return 0;
}

int MockMultiCard::MC_GetPrfPos(short axis, double* pos) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (pos == nullptr) {
        return -1;  // Invalid parameter
    }

    // Return mock position (0.0 for simplicity)
    *pos = 0.0;

    return 0;  // Success
}

int MockMultiCard::MC_GetSts(short axis, long* sts, short from_axis, unsigned long* clock) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (sts == nullptr) {
        return -1;  // Invalid parameter
    }

    // Increment call count for simulation
    getsts_call_count_++;

    // If forced return value is non-zero, use it (for testing degraded mode)
    // Zero means "use default behavior", non-zero means "force this return value"
    if (forced_getsts_return_value_ != 0) {
        return forced_getsts_return_value_;
    }

    // Simulate hardware disconnect after 5 calls (for heartbeat testing)
    if (simulate_disconnected_ || getsts_call_count_ > 5) {
        return -1;  // Simulate hardware not responding
    }

    *sts = 0;

    // Optionally set clock if provided
    if (clock != nullptr) {
        *clock = 0;
    }

    return 0;  // Success
}

// === Debug log (no-op) ===

void MockMultiCard::MC_StartDebugLog(int level) {
    // No-op in mock
}

// === Hardware status check ===

int MockMultiCard::MC_GetClock(double* pClock) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (pClock == nullptr) {
        return -1;  // Invalid parameter
    }

    // Check if any card is open
    bool any_open = false;
    for (const auto& pair : card_states_) {
        if (pair.second.is_open) {
            any_open = true;
            break;
        }
    }

    if (!any_open) {
        return -1;  // No card connected
    }

    // Return mock clock value
    *pClock = 0.0;
    return 0;  // Success
}

// === Verification methods ===

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

    return MockCardState();  // Return default state if not found
}

// === Testing control methods ===

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

// === Jog motion ===

int MockMultiCard::MC_HomeStop(short axis) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return 0;  // Success
}

int MockMultiCard::MC_PrfJog(short axis) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return 0;  // Success
}

int MockMultiCard::MC_SetJogPrm(short axis, void* jogPrm) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return 0;  // Success
}

// === Home setup ===

int MockMultiCard::MC_HomeSetPrm(short axis, void* homePrm) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return 0;  // Success
}

int MockMultiCard::MC_HomeSns(unsigned short sense) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    home_sense_mask_ = sense;
    return 0;  // Success
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
    return 0;  // Success
}

int MockMultiCard::MC_SetTrapPrm(short axis, void* trapPrm) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return 0;  // Success
}

// === CMP buffer operations ===

int MockMultiCard::MC_CmpRpt(
    short nCmpNum, unsigned long lIntervalTime, short nTime, short nTimeFlag, unsigned long ulRptTime) {
    std::lock_guard<std::mutex> lock(state_mutex_);

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
    return 0;  // Success
}

int MockMultiCard::MC_CmpBufSetChannel(short nBuf1ChannelNum, short nBuf2ChannelNum) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return 0;  // Success
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
    return 0;  // Success
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
    (void)nAbsPosFlag;
    (void)nTimerFlag;

    if (nBufLen1 > 0 && pBuf1 == nullptr) {
        return -1;  // Invalid parameter
    }
    if (nBufLen2 > 0 && pBuf2 == nullptr) {
        return -1;  // Invalid parameter
    }

    return 0;  // Success
}

int MockMultiCard::MC_CmpBufSts(unsigned short* pStatus,
                                unsigned short* pRemainData1,
                                unsigned short* pRemainData2,
                                unsigned short* pRemainSpace1,
                                unsigned short* pRemainSpace2) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (!pStatus || !pRemainData1 || !pRemainData2 || !pRemainSpace1 || !pRemainSpace2) {
        return -1;
    }
    *pStatus = 0;
    *pRemainData1 = 0;
    *pRemainData2 = 0;
    *pRemainSpace1 = 1000;
    *pRemainSpace2 = 1000;
    return 0;
}

// === Extended I/O operations ===

int MockMultiCard::MC_SetExtDoBit(short nCardIndex, short nBitIndex, unsigned short nValue) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return 0;  // Success
}

int MockMultiCard::MC_GetExtDoValue(short nCardIndex, unsigned long* pValue) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (pValue == nullptr) {
        return -1;  // Invalid parameter
    }

    *pValue = 0;
    return 0;  // Success
}

int MockMultiCard::MC_GetDiRaw(short nCardIndex, long* pValue) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (pValue == nullptr) {
        return -1;  // Invalid parameter
    }

    *pValue = 0;
    return 0;  // Success
}

// === Coordinate status ===

int MockMultiCard::MC_CrdStatus(short nCrdNum, short* pCrdStatus, long* pSegment, short FifoIndex) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (pCrdStatus == nullptr || pSegment == nullptr) {
        return -1;  // Invalid parameter
    }

    *pCrdStatus = 0;
    *pSegment = 0;
    return 0;  // Success
}

int MockMultiCard::MC_GetCrdPos(short nCrdNum, double* pPos) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (pPos == nullptr) {
        return -1;
    }
    (void)nCrdNum;
    std::fill(pPos, pPos + 8, 0.0);
    return 0;
}

int MockMultiCard::MC_GetCrdVel(short nCrdNum, double* pSynVel) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (pSynVel == nullptr) {
        return -1;
    }
    (void)nCrdNum;
    *pSynVel = 0.0;
    return 0;
}

// === Encoder operations ===

int MockMultiCard::MC_EncOn(short axis) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return 0;  // Success
}

int MockMultiCard::MC_EncOff(short axis) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return 0;  // Success
}

// === Time simulation ===

void MockMultiCard::TickMs(double dt_ms) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    simulation_time_ms_ += dt_ms;

    // Update coordinate system positions
    for (auto& [crd_id, crd_sys] : coordinate_systems_) {
        if (!crd_sys.is_running || crd_sys.current_segment >= crd_sys.trajectory_x.size()) {
            continue;
        }

        // Get target position
        long target_x = crd_sys.trajectory_x[crd_sys.current_segment];
        long target_y = crd_sys.trajectory_y[crd_sys.current_segment];

        // Get current position from mapped axes
        long axis_x = crd_sys.axis_map[0];  // X axis
        long axis_y = crd_sys.axis_map[1];  // Y axis

        long current_x = axes_[axis_x].position;
        long current_y = axes_[axis_y].position;

        // Calculate movement (simplified: 10mm/s = 10000 pulses/s = 10 pulses/ms)
        double velocity_pulses_per_ms = 10.0;
        double max_delta = velocity_pulses_per_ms * dt_ms;

        // Move towards target
        double dx = static_cast<double>(target_x - current_x);
        double dy = static_cast<double>(target_y - current_y);
        double distance = std::sqrt((dx * dx) + (dy * dy));

        if (distance <= max_delta) {
            // Reached target, move to next segment
            axes_[axis_x].position = target_x;
            axes_[axis_y].position = target_y;
            crd_sys.current_segment++;

            if (crd_sys.current_segment >= crd_sys.trajectory_x.size()) {
                crd_sys.is_running = false;  // All segments completed
            }
        } else {
            // Move towards target
            double ratio = max_delta / distance;
            axes_[axis_x].position += static_cast<long>(dx * ratio);
            axes_[axis_y].position += static_cast<long>(dy * ratio);
        }
    }

    // Generate CMP triggers
    for (auto& [cmp_id, trigger] : cmp_triggers_) {
        if (!trigger.enabled) {
            continue;
        }

        // Get current axis position
        long current_pos = axes_[trigger.axis].position;

        // Check if we should trigger
        if (current_pos >= trigger.start_pos) {
            long offset = current_pos - trigger.start_pos;
            long trigger_index = offset / trigger.interval;

            // Check if this is a new trigger
            if (trigger_index < trigger.count &&
                trigger.trigger_times.size() <= static_cast<size_t>(trigger_index)) {
                // Record trigger time
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
        return {};  // No trigger configured
    }

    return it->second.trigger_times;
}


// 速度控制
int Siligen::Infrastructure::Hardware::MockMultiCard::MC_SetOverride(short crd, double ratio) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (coordinate_systems_.find(crd) != coordinate_systems_.end()) {
        coordinate_systems_[crd].velocity_override = ratio;
    }
    return 0;
}

int Siligen::Infrastructure::Hardware::MockMultiCard::MC_G00SetOverride(short crd, double ratio) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (coordinate_systems_.find(crd) != coordinate_systems_.end()) {
        coordinate_systems_[crd].g00_velocity_override = ratio;
    }
    return 0;
}

int Siligen::Infrastructure::Hardware::MockMultiCard::MC_CrdSMoveEnable(short crd, double jerk) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (coordinate_systems_.find(crd) != coordinate_systems_.end()) {
        coordinate_systems_[crd].s_curve_enabled = true;
        coordinate_systems_[crd].jerk = jerk;
    }
    return 0;
}

int Siligen::Infrastructure::Hardware::MockMultiCard::MC_CrdSMoveDisable(short crd) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (coordinate_systems_.find(crd) != coordinate_systems_.end()) {
        coordinate_systems_[crd].s_curve_enabled = false;
    }
    return 0;
}

}  // namespace Hardware
}  // namespace Infrastructure
}  // namespace Siligen
