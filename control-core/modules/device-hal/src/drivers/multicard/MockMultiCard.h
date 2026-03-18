#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <cstdint>

namespace Siligen {
namespace Infrastructure {
namespace Hardware {

/// Mock card connection state
struct MockCardState {
    bool is_open = false;
    bool is_reset = false;
    std::string local_ip;
    std::string card_ip;
    uint16_t local_port = 0;
    uint16_t card_port = 0;
};

/**
 * @brief MockMultiCard - Behavior-compatible mock of MultiCard C++ class
 *
 * This class provides a mock implementation of the MultiCard hardware interface
 * for testing purposes. It mimics the MultiCard C++ class API without requiring
 * actual hardware connection.
 */
class MockMultiCard {
public:
    MockMultiCard();
    virtual ~MockMultiCard();

    // Core connection API (mirroring MultiCard)
    int MC_Open(short nCardNum, char* cPCEthernetIP, unsigned short nPCEthernetPort,
               char* cCardEthernetIP, unsigned short nCardEthernetPort);
    int MC_Close(void);
    int MC_Reset();
    int MC_ResetAllM();

    // Axis control stubs (return success for now)
    int MC_AxisOn(short axis, short profile = 0);
    int MC_AxisOff(short axis, short profile = 0);
    int MC_SetPos(short axis, long position);
    int MC_GetPos(short axis, long* position);
    int MC_GetAxisStatus(short axis, unsigned long* status);
    int MC_HomeStart(short axis);
    int MC_Stop(long mask, short mode);
    int MC_StopEx(long crd_mask, long crd_option, long axis_mask, long axis_option);

    // Jog control
    int MC_PrfJog(short axis);
    int MC_SetJogPrm(short axis, void* jogPrm);

    // Home control
    int MC_HomeSetPrm(short axis, void* homePrm);
    int MC_HomeSns(unsigned short sense);
    int MC_GetHomeSns(unsigned short* sense);
    int MC_HomeGetSts(short axis, unsigned short* status);
    int MC_HomeStop(short axis);
    int MC_ClrSts(short axis, short count = 1);

    // Trap motion (position mode)
    int MC_PrfTrap(short axis);
    // Trap motion parameters
    int MC_SetTrapPrm(short axis, void* trapPrm);
    int MC_SetVel(short axis, double velocity);
    int MC_Update(long mask);

    // Coordinate motion stubs
    int MC_CrdSpace(short crd, long* space, short fifo);
    int MC_GetCrdSpace(short crd, long* space);
    int MC_CrdClear(short crd, short fifo);
    int MC_LnXY(short crd, long x, long y, double synVel, double synAcc, double endVel, short fifo, long segment_id = 0);
    int MC_ArcXYC(short crd, long x, long y, double cx, double cy, short direction, double synVel, double synAcc, double endVel, short fifo, long segment_id = 0);
    int MC_ArcXYR(short crd, long x, long y, double radius, short direction, double synVel, double synAcc, double endVel, short fifo);
    int MC_CrdData(short nCrdNum, void* pCrdData, short nFifoIndex = 0);
    int MC_CrdStart(short crd, short mask);
    int MC_CmpPluse(unsigned short channel, unsigned short crd, long start_pos, long interval, unsigned short count, unsigned short output_mode, unsigned short output_inverse);
    int MC_CmpRpt(short nCmpNum, unsigned long lIntervalTime, short nTime, short nTimeFlag, unsigned long ulRptTime);
    int MC_CmpBufStop(unsigned short channelMask);
    int MC_CmpBufSetChannel(short nBuf1ChannelNum, short nBuf2ChannelNum);
    int MC_CmpBufRpt(short nEncNum, short nDir, short nEncFlag, long lTrigValue, short nCmpNum, unsigned long lIntervalTime, short nTime, short nTimeFlag, unsigned long ulRptTime);
    int MC_GetPrfPos(short axis, double* pos);
    int MC_GetSts(short axis, long* sts, short from_axis = 1, unsigned long* clock = nullptr);
    int MC_GetClock(double* pClock);

    // I/O control
    int MC_SetExtDoBit(short nCardIndex, short nBitIndex, unsigned short nValue);
    int MC_GetExtDoValue(short nCardIndex, unsigned long* pValue);
    int MC_GetDiRaw(short nCardIndex, long* pValue);
    int MC_CrdStatus(short nCrdNum, short* pCrdStatus, long* pSegment, short FifoIndex = 0);
    int MC_GetCrdPos(short nCrdNum, double* pPos);
    int MC_GetCrdVel(short nCrdNum, double* pSynVel);
    int MC_CmpBufData(short nCmpEncodeNum, short nPluseType, short nStartLevel, short nTime, long* pBuf1, short nBufLen1, long* pBuf2, short nBufLen2, short nAbsPosFlag = 0, short nTimerFlag = 0);
    int MC_CmpBufSts(unsigned short* pStatus,
                     unsigned short* pRemainData1,
                     unsigned short* pRemainData2,
                     unsigned short* pRemainSpace1,
                     unsigned short* pRemainSpace2);

    // Encoder control
    int MC_EncOn(short axis);
    int MC_EncOff(short axis);
    
    // 速度控制
    int MC_SetOverride(short crd, double ratio);
    int MC_G00SetOverride(short crd, double ratio);
    int MC_CrdSMoveEnable(short crd, double jerk);
    int MC_CrdSMoveDisable(short crd);
    int MC_SetConstLinearVelFlag(short crd, short flag);
    int MC_GetLookAheadSpace(short crd, long* space, short fifo = 0);

    // Debug log (no-op in mock)
    void MC_StartDebugLog(int level);

    // Verification methods (for testing)
    bool IsCardOpen(short cardNum) const;
    MockCardState GetCardState(short cardNum) const;
    int GetOpenCallCount() const { return open_call_count_; }
    int GetCloseCallCount() const { return close_call_count_; }
    int GetResetCallCount() const { return reset_call_count_; }
    int GetGetStsCallCount() const { return getsts_call_count_; }

    // Testing control methods
    void SimulateDisconnect();  // Simulate hardware power-off after N MC_GetSts calls
    void ResetGetStsCounter();  // Reset MC_GetSts call counter (for testing)
    void SetGetStsReturnValue(int return_value);  // Set MC_GetSts return value for testing

    // Time simulation
    void TickMs(double dt_ms);
    double GetSimulationTime() const;

    // Test verification interface
    std::vector<long> GetCMPTriggerTimes(short cmp) const;

private:
    struct AxisState {
        long position = 0;
    };

    struct CoordinateSystem {
        bool configured = false;
        long axis_map[4] = {0, 0, 0, 0};  // Maps crd axes to physical axes
        std::vector<long> trajectory_x;    // Buffered X positions
        std::vector<long> trajectory_y;    // Buffered Y positions

        // Arc segment information
        struct ArcSegment {
            long center_x;
            long center_y;
            short direction;  // 0=CW, 1=CCW, -1=linear
        };
        std::vector<ArcSegment> arc_segments;

        bool is_running = false;
        double velocity_override = 1.0;
        double g00_velocity_override = 1.0;
        bool s_curve_enabled = false;
        double jerk = 0.0;
        bool const_linear_vel_enabled = false;
        long lookahead_space = 1000;
        size_t current_segment = 0;
    };

    // CMP trigger state
    struct CMPTrigger {
        bool enabled = false;
        short axis = 0;
        long start_pos = 0;
        long interval = 0;
        long count = 0;
        std::vector<long> trigger_times;  // Simulation times when triggers occurred
    };

    double simulation_time_ms_ = 0.0;  // Simulation time in milliseconds
    int getsts_call_count_ = 0;  // Track MC_GetSts calls
    bool simulate_disconnected_ = false;  // Flag to simulate disconnection
    int forced_getsts_return_value_ = 0;  // Forced return value (default: 0 for success, set to -999 for degraded mode testing)

    mutable std::mutex state_mutex_;
    std::map<short, MockCardState> card_states_;
    std::map<short, CoordinateSystem> coordinate_systems_;
    std::map<short, AxisState> axes_;
    std::map<short, CMPTrigger> cmp_triggers_;  // Map from cmp_id to trigger state
    int open_call_count_ = 0;
    int close_call_count_ = 0;
    int reset_call_count_ = 0;
    unsigned short home_sense_mask_ = 0;
};

} // namespace Hardware
} // namespace Infrastructure
} // namespace Siligen
