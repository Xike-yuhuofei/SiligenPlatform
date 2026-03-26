#include "siligen/device/adapters/drivers/multicard/RealMultiCardWrapper.h"

namespace Siligen {
namespace Infrastructure {
namespace Hardware {

RealMultiCardWrapper::RealMultiCardWrapper(std::shared_ptr<MultiCard> multicard)
    : multicard_(multicard ? multicard : std::make_shared<MultiCard>()) {}

// ========== 连接管理 ==========

int RealMultiCardWrapper::MC_Open(short nCardNum,
                                  char* cPCEthernetIP,
                                  unsigned short nPCEthernetPort,
                                  char* cCardEthernetIP,
                                  unsigned short nCardEthernetPort) noexcept {
    return multicard_->MC_Open(nCardNum, cPCEthernetIP, nPCEthernetPort, cCardEthernetIP, nCardEthernetPort);
}

int RealMultiCardWrapper::MC_Close() noexcept {
    return multicard_->MC_Close();
}

int RealMultiCardWrapper::MC_Reset() noexcept {
    return multicard_->MC_Reset();
}

int RealMultiCardWrapper::MC_ResetAllM() noexcept {
    return multicard_->MC_ResetAllM();
}

// ========== 轴控制 ==========

int RealMultiCardWrapper::MC_AxisOn(short axis) noexcept {
    return multicard_->MC_AxisOn(axis);
}

int RealMultiCardWrapper::MC_AxisOff(short axis) noexcept {
    return multicard_->MC_AxisOff(axis);
}

int RealMultiCardWrapper::MC_SetPos(short axis, long position) noexcept {
    return multicard_->MC_SetPos(axis, position);
}

int RealMultiCardWrapper::MC_GetPos(short axis, long* position) noexcept {
    return multicard_->MC_GetPos(axis, position);
}

int RealMultiCardWrapper::MC_GetSts(short axis, long* status, short from_axis, unsigned long* clock) noexcept {
    return multicard_->MC_GetSts(axis, status, from_axis, clock);
}

int RealMultiCardWrapper::MC_GetPrfPos(short axis, double* pos) noexcept {
    return multicard_->MC_GetPrfPos(axis, pos);
}

int RealMultiCardWrapper::MC_GetAxisEncPos(short axis, double* pos) noexcept {
    return multicard_->MC_GetAxisEncPos(axis, pos);
}

int RealMultiCardWrapper::MC_GetAxisPrfVel(short axis, double* vel, short count, unsigned long* clock) noexcept {
    return multicard_->MC_GetAxisPrfVel(axis, vel, count, clock);
}

int RealMultiCardWrapper::MC_GetAxisEncVel(short axis, double* vel, short count, unsigned long* clock) noexcept {
    return multicard_->MC_GetAxisEncVel(axis, vel, count, clock);
}

int RealMultiCardWrapper::MC_GetAlarmOnOff(short axis, short* alarmOnOff) noexcept {
    return multicard_->MC_GetAlarmOnOff(axis, alarmOnOff);
}

// ========== 运动控制 ==========

int RealMultiCardWrapper::MC_SetVel(short axis, double velocity) noexcept {
    return multicard_->MC_SetVel(axis, velocity);
}

int RealMultiCardWrapper::MC_Update(long mask) noexcept {
    return multicard_->MC_Update(mask);
}

int RealMultiCardWrapper::MC_Stop(long mask, short mode) noexcept {
    return multicard_->MC_Stop(mask, mode);
}

int RealMultiCardWrapper::MC_StopEx(long mask, short stopMode) noexcept {
    return multicard_->MC_Stop(mask, stopMode);
}

// ========== 点动控制 ==========

int RealMultiCardWrapper::MC_PrfJog(short axis) noexcept {
    return multicard_->MC_PrfJog(axis);
}

int RealMultiCardWrapper::MC_SetJogPrm(short axis, void* jogPrm) noexcept {
    return multicard_->MC_SetJogPrm(axis, static_cast<TJogPrm*>(jogPrm));
}

// ========== 回零控制 ==========

int RealMultiCardWrapper::MC_HomeSetPrm(short axis, void* homePrm) noexcept {
    return multicard_->MC_HomeSetPrm(axis, static_cast<TAxisHomePrm*>(homePrm));
}

int RealMultiCardWrapper::MC_HomeSns(unsigned short sense) noexcept {
    return multicard_->MC_HomeSns(sense);
}

int RealMultiCardWrapper::MC_GetHomeSns(unsigned short* sense) noexcept {
    if (!sense) {
        return -1;
    }
    unsigned long value = 0;
    int ret = multicard_->MC_GetHomeSns(&value);
    if (ret != 0) {
        return ret;
    }
    *sense = static_cast<unsigned short>(value & 0xFFFF);
    return 0;
}

int RealMultiCardWrapper::MC_HomeStart(short axis) noexcept {
    return multicard_->MC_HomeStart(axis);
}

int RealMultiCardWrapper::MC_HomeGetSts(short axis, unsigned short* status) noexcept {
    return multicard_->MC_HomeGetSts(axis, status);
}

int RealMultiCardWrapper::MC_HomeStop(short axis) noexcept {
    return multicard_->MC_HomeStop(axis);
}

int RealMultiCardWrapper::MC_ClrSts(short axis, short count) noexcept {
    return multicard_->MC_ClrSts(axis, count);
}

// ========== 轮廓运动 ==========

int RealMultiCardWrapper::MC_PrfTrap(short axis) noexcept {
    return multicard_->MC_PrfTrap(axis);
}

int RealMultiCardWrapper::MC_SetTrapPrm(short axis, void* trapPrm) noexcept {
    return multicard_->MC_SetTrapPrm(axis, static_cast<TTrapPrm*>(trapPrm));
}

// ========== 坐标系运动 ==========

int RealMultiCardWrapper::MC_CrdSpace(short crd, long* space, short fifo) noexcept {
    return multicard_->MC_CrdSpace(crd, space, fifo);
}

int RealMultiCardWrapper::MC_CrdClear(short crd, short fifo) noexcept {
    return multicard_->MC_CrdClear(crd, fifo);
}

int RealMultiCardWrapper::MC_SetCrdPrm(short nCrdNum, short dimension, short* profile,
                                        double synVelMax, double synAccMax) noexcept {
    // 构造 TCrdPrm 结构体
    TCrdPrm crdPrm = {};
    crdPrm.dimension = dimension;
    for (int i = 0; i < dimension && i < 8; ++i) {
        crdPrm.profile[i] = profile[i];
    }
    crdPrm.synVelMax = synVelMax;
    crdPrm.synAccMax = synAccMax;
    crdPrm.evenTime = 0;        // 默认值
    crdPrm.setOriginFlag = 0;   // 使用当前规划位置作为原点
    // originPos 默认为 0

    return multicard_->MC_SetCrdPrm(nCrdNum, &crdPrm);
}

int RealMultiCardWrapper::MC_LnXY(
    short crd, long x, long y, double synVel, double synAcc, double endVel, short fifo, long segment_id) noexcept {
    return multicard_->MC_LnXY(crd, x, y, synVel, synAcc, endVel, fifo, segment_id);
}

int RealMultiCardWrapper::MC_ArcXYC(short crd,
                                    long x,
                                    long y,
                                    double cx,
                                    double cy,
                                    short direction,
                                    double synVel,
                                    double synAcc,
                                    double endVel,
                                    short fifo,
                                    long segment_id) noexcept {
    return multicard_->MC_ArcXYC(crd, x, y, cx, cy, direction, synVel, synAcc, endVel, fifo, segment_id);
}

int RealMultiCardWrapper::MC_CrdData(short nCrdNum, void* pCrdData, short fifoIndex) noexcept {
    return multicard_->MC_CrdData(nCrdNum, pCrdData, fifoIndex);
}

int RealMultiCardWrapper::MC_CrdStart(short crd, short mask) noexcept {
    return multicard_->MC_CrdStart(crd, mask);
}

int RealMultiCardWrapper::MC_InitLookAhead(short nCrdNum,
                                            short fifoIndex,
                                            int lookAheadNum,
                                            double* speedMax,
                                            double* accMax,
                                            double* maxStepSpeed,
                                            double* scale) noexcept {
    // 构造 TLookAheadPrm 结构体
    TLookAheadPrm lookAheadPrm = {};
    lookAheadPrm.lookAheadNum = lookAheadNum;

    // 复制各轴参数（最多32轴，但通常使用前8轴）
    for (int i = 0; i < 32; ++i) {
        if (i < 8 && speedMax) {
            lookAheadPrm.dSpeedMax[i] = speedMax[i];
        }
        if (i < 8 && accMax) {
            lookAheadPrm.dAccMax[i] = accMax[i];
        }
        if (i < 8 && maxStepSpeed) {
            lookAheadPrm.dMaxStepSpeed[i] = maxStepSpeed[i];
        }
        if (i < 8 && scale) {
            lookAheadPrm.dScale[i] = scale[i];
        }
    }

    if (lookAheadNum > 0) {
        const int key = (static_cast<int>(nCrdNum) << 1) | (fifoIndex & 0x1);
        auto& buffer = lookahead_buffers_[key];
        buffer.assign(static_cast<size_t>(lookAheadNum), TCrdData{});
        lookAheadPrm.pLookAheadBuf = buffer.data();
    } else {
        lookAheadPrm.pLookAheadBuf = nullptr;
    }

    return multicard_->MC_InitLookAhead(nCrdNum, fifoIndex, &lookAheadPrm);
}

int Siligen::Infrastructure::Hardware::RealMultiCardWrapper::MC_CrdSMoveEnable(short nCrdNum, double dJ) noexcept {
    return multicard_->MC_CrdSMoveEnable(nCrdNum, dJ);
}

int Siligen::Infrastructure::Hardware::RealMultiCardWrapper::MC_CrdSMoveDisable(short nCrdNum) noexcept {
    return multicard_->MC_CrdSMoveDisable(nCrdNum);
}

int Siligen::Infrastructure::Hardware::RealMultiCardWrapper::MC_SetConstLinearVelFlag(short nCrdNum,
                                                   short nFifoIndex,
                                                   short nConstLinearVelFlag,
                                                   long lRotateAxisMask) noexcept {
    // NOTE: MC_SetConstLinearVelFlag not available in current MultiCard.lib version
    // Return 0 (success) as this is an optional optimization feature
    (void)nCrdNum;
    (void)nFifoIndex;
    (void)nConstLinearVelFlag;
    (void)lRotateAxisMask;
    return 0;
}

int Siligen::Infrastructure::Hardware::RealMultiCardWrapper::MC_GetLookAheadSpace(short nCrdNum, long* pSpace, short FifoIndex) noexcept {
    return multicard_->MC_GetLookAheadSpace(nCrdNum, pSpace, FifoIndex);
}

int RealMultiCardWrapper::MC_SetOverride(short nCrdNum, double synVelRatio) noexcept {
    return multicard_->MC_SetOverride(nCrdNum, synVelRatio);
}

int Siligen::Infrastructure::Hardware::RealMultiCardWrapper::MC_G00SetOverride(short nCrdNum, double synVelRatio) noexcept {
    return multicard_->MC_G00SetOverride(nCrdNum, synVelRatio);
}

// ========== CMP 触发 ==========

int RealMultiCardWrapper::MC_CmpPluse(short nChannelMask,
                                      short nPluseType1,
                                      short nPluseType2,
                                      short nTime1,
                                      short nTime2,
                                      short nTimeFlag1,
                                      short nTimeFlag2) noexcept {
    return multicard_->MC_CmpPluse(nChannelMask, nPluseType1, nPluseType2, nTime1, nTime2, nTimeFlag1, nTimeFlag2);
}

int RealMultiCardWrapper::MC_CmpRpt(
    short nCmpNum, unsigned long lIntervalTime, short nTime, short nTimeFlag, unsigned long ulRptTime) noexcept {
    // COMPILE FIX: MC_CmpRpt signature mismatch in MultiCard.lib
    // TODO: Update MultiCard.lib or verify correct parameter types
    (void)nCmpNum;
    (void)lIntervalTime;
    (void)nTime;
    (void)nTimeFlag;
    (void)ulRptTime;
    return -1;  // Error: function not implemented in current hardware library version
}

int RealMultiCardWrapper::MC_CmpBufStop(unsigned short channelMask) noexcept {
    return multicard_->MC_CmpBufStop(channelMask);
}

int RealMultiCardWrapper::MC_CmpBufSetChannel(short nBuf1ChannelNum, short nBuf2ChannelNum) noexcept {
    return multicard_->MC_CmpBufSetChannel(nBuf1ChannelNum, nBuf2ChannelNum);
}

int RealMultiCardWrapper::MC_CmpBufRpt(short nEncNum,
                                       short nDir,
                                       short nEncFlag,
                                       long lTrigValue,
                                       short nCmpNum,
                                       unsigned long lIntervalTime,
                                       short nTime,
                                       short nTimeFlag,
                                       unsigned long ulRptTime) noexcept {
    // COMPILE FIX: MC_CmpBufRpt not found in MultiCard.lib
    // TODO: Update MultiCard.lib or use alternative function (MC_CmpRpt)
    (void)nEncNum;
    (void)nDir;
    (void)nEncFlag;
    (void)lTrigValue;
    (void)nCmpNum;
    (void)lIntervalTime;
    (void)nTime;
    (void)nTimeFlag;
    (void)ulRptTime;
    return -1;  // Error: function not implemented in hardware library
}

// ========== I/O 控制 ==========

int RealMultiCardWrapper::MC_SetExtDoBit(short nCardIndex, short nBitIndex, unsigned short nValue) noexcept {
    return multicard_->MC_SetExtDoBit(nCardIndex, nBitIndex, nValue);
}

int RealMultiCardWrapper::MC_GetExtDoValue(short nCardIndex, unsigned long* pValue) noexcept {
    return multicard_->MC_GetExtDoValue(nCardIndex, pValue);
}

int RealMultiCardWrapper::MC_GetDiRaw(short nCardIndex, long* pValue) noexcept {
    return multicard_->MC_GetDiRaw(nCardIndex, pValue);
}

int RealMultiCardWrapper::MC_CrdStatus(short nCrdNum, short* pCrdStatus, long* pSegment, short FifoIndex) noexcept {
    return multicard_->MC_CrdStatus(nCrdNum, pCrdStatus, pSegment, FifoIndex);
}

int RealMultiCardWrapper::MC_GetCrdPos(short nCrdNum, double* pPos) noexcept {
    return multicard_->MC_GetCrdPos(nCrdNum, pPos);
}

int RealMultiCardWrapper::MC_GetCrdVel(short nCrdNum, double* pSynVel) noexcept {
    return multicard_->MC_GetCrdVel(nCrdNum, pSynVel);
}

int RealMultiCardWrapper::MC_CmpBufData(short nCmpEncodeNum,
                                        short nPluseType,
                                        short nStartLevel,
                                        short nTime,
                                        long* pBuf1,
                                        short nBufLen1,
                                        long* pBuf2,
                                        short nBufLen2,
                                        short nAbsPosFlag,
                                        short nTimerFlag) noexcept {
    return multicard_->MC_CmpBufData(
        nCmpEncodeNum, nPluseType, nStartLevel, nTime, pBuf1, nBufLen1, pBuf2, nBufLen2, nAbsPosFlag, nTimerFlag);
}

int RealMultiCardWrapper::MC_CmpBufSts(unsigned short* pStatus,
                                       unsigned short* pRemainData1,
                                       unsigned short* pRemainData2,
                                       unsigned short* pRemainSpace1,
                                       unsigned short* pRemainSpace2) noexcept {
    return multicard_->MC_CmpBufSts(pStatus, pRemainData1, pRemainData2, pRemainSpace1, pRemainSpace2);
}

// ========== 限位控制 ==========

int RealMultiCardWrapper::MC_SetHardLimP(short axis, short type, short cardIndex, short ioIndex) noexcept {
    return multicard_->MC_SetHardLimP(axis, type, cardIndex, ioIndex);
}

int RealMultiCardWrapper::MC_SetHardLimN(short axis, short type, short cardIndex, short ioIndex) noexcept {
    return multicard_->MC_SetHardLimN(axis, type, cardIndex, ioIndex);
}

int RealMultiCardWrapper::MC_LmtsOn(short axis, short limitType) noexcept {
    return multicard_->MC_LmtsOn(axis, limitType);
}

int RealMultiCardWrapper::MC_LmtSns(unsigned short sense) noexcept {
    return multicard_->MC_LmtSns(sense);
}

int RealMultiCardWrapper::MC_LmtsOff(short axis, short limitType) noexcept {
    return multicard_->MC_LmtsOff(axis, limitType);
}

int RealMultiCardWrapper::MC_GetLmtsOnOff(short axis, short* posLimitEnabled, short* negLimitEnabled) noexcept {
    return multicard_->MC_GetLmtsOnOff(axis, posLimitEnabled, negLimitEnabled);
}

int RealMultiCardWrapper::MC_SetLmtSnsSingle(short axis, short posPolarity, short negPolarity) noexcept {
    return multicard_->MC_SetLmtSnsSingle(axis, posPolarity, negPolarity);
}

int RealMultiCardWrapper::MC_SetSoftLimit(short cardIndex, short axis, long posLimit, long negLimit) noexcept {
    return multicard_->MC_SetSoftLimit(axis, posLimit, negLimit);
}

// ========== 编码器控制 ==========

int RealMultiCardWrapper::MC_EncOn(short axis) noexcept {
    return multicard_->MC_EncOn(axis);
}

int RealMultiCardWrapper::MC_EncOff(short axis) noexcept {
    return multicard_->MC_EncOff(axis);
}

// ========== 调试支持 ==========

void RealMultiCardWrapper::MC_StartDebugLog(int level) noexcept {
    multicard_->MC_StartDebugLog(level);
}

// ========== 底层访问 ==========

void* RealMultiCardWrapper::GetUnderlyingMultiCard() noexcept {
    return multicard_.get();
}

}  // namespace Hardware
}  // namespace Infrastructure
}  // namespace Siligen

