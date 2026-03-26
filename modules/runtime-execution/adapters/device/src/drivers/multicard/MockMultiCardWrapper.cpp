#include "siligen/device/adapters/drivers/multicard/MockMultiCardWrapper.h"

namespace Siligen {
namespace Infrastructure {
namespace Hardware {

MockMultiCardWrapper::MockMultiCardWrapper(std::shared_ptr<MockMultiCard> mockMulticard)
    : mockMulticard_(mockMulticard ? mockMulticard : std::make_shared<MockMultiCard>()) {}

// ========== 连接管理 ==========

int MockMultiCardWrapper::MC_Open(short nCardNum,
                                  char* cPCEthernetIP,
                                  unsigned short nPCEthernetPort,
                                  char* cCardEthernetIP,
                                  unsigned short nCardEthernetPort) noexcept {
    return mockMulticard_->MC_Open(nCardNum, cPCEthernetIP, nPCEthernetPort, cCardEthernetIP, nCardEthernetPort);
}

int MockMultiCardWrapper::MC_Close() noexcept {
    return mockMulticard_->MC_Close();
}

int MockMultiCardWrapper::MC_Reset() noexcept {
    return mockMulticard_->MC_Reset();
}

int MockMultiCardWrapper::MC_ResetAllM() noexcept {
    return mockMulticard_->MC_ResetAllM();
}

// ========== 轴控制 ==========

int MockMultiCardWrapper::MC_AxisOn(short axis) noexcept {
    return mockMulticard_->MC_AxisOn(axis);
}

int MockMultiCardWrapper::MC_AxisOff(short axis) noexcept {
    return mockMulticard_->MC_AxisOff(axis);
}

int MockMultiCardWrapper::MC_SetPos(short axis, long position) noexcept {
    return mockMulticard_->MC_SetPos(axis, position);
}

int MockMultiCardWrapper::MC_GetPos(short axis, long* position) noexcept {
    return mockMulticard_->MC_GetPos(axis, position);
}

int MockMultiCardWrapper::MC_GetSts(short axis, long* status, short from_axis, unsigned long* clock) noexcept {
    return mockMulticard_->MC_GetSts(axis, status, from_axis, clock);
}

int MockMultiCardWrapper::MC_GetPrfPos(short axis, double* pos) noexcept {
    return mockMulticard_->MC_GetPrfPos(axis, pos);
}

int MockMultiCardWrapper::MC_GetAxisEncPos(short axis, double* pos) noexcept {
    return mockMulticard_->MC_GetAxisEncPos(axis, pos);
}

int MockMultiCardWrapper::MC_GetAxisPrfVel(short axis, double* vel, short count, unsigned long* clock) noexcept {
    return mockMulticard_->MC_GetAxisPrfVel(axis, vel, count, clock);
}

int MockMultiCardWrapper::MC_GetAxisEncVel(short axis, double* vel, short count, unsigned long* clock) noexcept {
    return mockMulticard_->MC_GetAxisEncVel(axis, vel, count, clock);
}

int MockMultiCardWrapper::MC_GetAlarmOnOff(short axis, short* alarmOnOff) noexcept {
    return mockMulticard_->MC_GetAlarmOnOff(axis, alarmOnOff);
}

// ========== 运动控制 ==========

int MockMultiCardWrapper::MC_SetVel(short axis, double velocity) noexcept {
    return mockMulticard_->MC_SetVel(axis, velocity);
}

int MockMultiCardWrapper::MC_Update(long mask) noexcept {
    return mockMulticard_->MC_Update(mask);
}

int MockMultiCardWrapper::MC_Stop(long mask, short mode) noexcept {
    return mockMulticard_->MC_Stop(mask, mode);
}

int MockMultiCardWrapper::MC_StopEx(long mask, short stopMode) noexcept {
    return mockMulticard_->MC_Stop(mask, stopMode);
}

// ========== 点动控制 ==========

int MockMultiCardWrapper::MC_PrfJog(short axis) noexcept {
    return mockMulticard_->MC_PrfJog(axis);
}

int MockMultiCardWrapper::MC_SetJogPrm(short axis, void* jogPrm) noexcept {
    return mockMulticard_->MC_SetJogPrm(axis, static_cast<TJogPrm*>(jogPrm));
}

// ========== 回零控制 ==========

int MockMultiCardWrapper::MC_HomeSetPrm(short axis, void* homePrm) noexcept {
    return mockMulticard_->MC_HomeSetPrm(axis, static_cast<TAxisHomePrm*>(homePrm));
}

int MockMultiCardWrapper::MC_HomeSns(unsigned short sense) noexcept {
    return mockMulticard_->MC_HomeSns(sense);
}

int MockMultiCardWrapper::MC_GetHomeSns(unsigned short* sense) noexcept {
    return mockMulticard_->MC_GetHomeSns(sense);
}

int MockMultiCardWrapper::MC_HomeStart(short axis) noexcept {
    return mockMulticard_->MC_HomeStart(axis);
}

int MockMultiCardWrapper::MC_HomeGetSts(short axis, unsigned short* status) noexcept {
    return mockMulticard_->MC_HomeGetSts(axis, status);
}

int MockMultiCardWrapper::MC_HomeStop(short axis) noexcept {
    return mockMulticard_->MC_HomeStop(axis);
}

int MockMultiCardWrapper::MC_ClrSts(short axis, short count) noexcept {
    return mockMulticard_->MC_ClrSts(axis, count);
}

// ========== 轮廓运动 ==========

int MockMultiCardWrapper::MC_PrfTrap(short axis) noexcept {
    return mockMulticard_->MC_PrfTrap(axis);
}

int MockMultiCardWrapper::MC_SetTrapPrm(short axis, void* trapPrm) noexcept {
    return mockMulticard_->MC_SetTrapPrm(axis, trapPrm);
}

// ========== 坐标系运动 ==========

int MockMultiCardWrapper::MC_CrdSpace(short crd, long* space, short fifo) noexcept {
    return mockMulticard_->MC_CrdSpace(crd, space, fifo);
}

int MockMultiCardWrapper::MC_CrdClear(short crd, short fifo) noexcept {
    return mockMulticard_->MC_CrdClear(crd, fifo);
}

int MockMultiCardWrapper::MC_SetCrdPrm(short nCrdNum, short dimension, short* profile,
                                        double synVelMax, double synAccMax) noexcept {
    // Mock 实现：简单返回成功
    (void)nCrdNum;
    (void)dimension;
    (void)profile;
    (void)synVelMax;
    (void)synAccMax;
    return 0;
}

int MockMultiCardWrapper::MC_LnXY(
    short crd, long x, long y, double synVel, double synAcc, double endVel, short fifo, long segment_id) noexcept {
    return mockMulticard_->MC_LnXY(crd, x, y, synVel, synAcc, endVel, fifo, segment_id);
}

int MockMultiCardWrapper::MC_ArcXYC(short crd,
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
    return mockMulticard_->MC_ArcXYC(crd, x, y, cx, cy, direction, synVel, synAcc, endVel, fifo, segment_id);
}

int MockMultiCardWrapper::MC_CrdData(short nCrdNum, void* pCrdData, short fifoIndex) noexcept {
    return mockMulticard_->MC_CrdData(nCrdNum, pCrdData, fifoIndex);
}

int MockMultiCardWrapper::MC_CrdStart(short crd, short mask) noexcept {
    return mockMulticard_->MC_CrdStart(crd, mask);
}

int MockMultiCardWrapper::MC_InitLookAhead(short nCrdNum,
                                            short fifoIndex,
                                            int lookAheadNum,
                                            double* speedMax,
                                            double* accMax,
                                            double* maxStepSpeed,
                                            double* scale) noexcept {
    // Mock 实现：简单返回成功
    (void)nCrdNum;
    (void)fifoIndex;
    (void)lookAheadNum;
    (void)speedMax;
    (void)accMax;
    (void)maxStepSpeed;
    (void)scale;
    return 0;
}

int Siligen::Infrastructure::Hardware::MockMultiCardWrapper::MC_CrdSMoveEnable(short nCrdNum, double dJ) noexcept {
    (void)nCrdNum;
    (void)dJ;
    return 0;
}

int Siligen::Infrastructure::Hardware::MockMultiCardWrapper::MC_CrdSMoveDisable(short nCrdNum) noexcept {
    (void)nCrdNum;
    return 0;
}

int Siligen::Infrastructure::Hardware::MockMultiCardWrapper::MC_SetConstLinearVelFlag(short nCrdNum,
                                                   short nFifoIndex,
                                                   short nConstLinearVelFlag,
                                                   long lRotateAxisMask) noexcept {
    (void)nCrdNum;
    (void)nFifoIndex;
    (void)nConstLinearVelFlag;
    (void)lRotateAxisMask;
    return 0;
}

int Siligen::Infrastructure::Hardware::MockMultiCardWrapper::MC_GetLookAheadSpace(short nCrdNum, long* pSpace, short FifoIndex) noexcept {
    (void)nCrdNum;
    (void)FifoIndex;
    if (pSpace) {
        *pSpace = 0;
    }
    return 0;
}

int MockMultiCardWrapper::MC_SetOverride(short nCrdNum, double synVelRatio) noexcept {
    (void)nCrdNum;
    (void)synVelRatio;
    return 0;
}

int Siligen::Infrastructure::Hardware::MockMultiCardWrapper::MC_G00SetOverride(short nCrdNum, double synVelRatio) noexcept {
    (void)nCrdNum;
    (void)synVelRatio;
    return 0;
}

// ========== CMP 触发 ==========

int MockMultiCardWrapper::MC_CmpPluse(short nChannelMask,
                                      short nPluseType1,
                                      short nPluseType2,
                                      short nTime1,
                                      short nTime2,
                                      short nTimeFlag1,
                                      short nTimeFlag2) noexcept {
    return mockMulticard_->MC_CmpPluse(nChannelMask, nPluseType1, nPluseType2, nTime1, nTime2, nTimeFlag1, nTimeFlag2);
}

int MockMultiCardWrapper::MC_CmpRpt(
    short nCmpNum, unsigned long lIntervalTime, short nTime, short nTimeFlag, unsigned long ulRptTime) noexcept {
    return mockMulticard_->MC_CmpRpt(nCmpNum, lIntervalTime, nTime, nTimeFlag, ulRptTime);
}
int MockMultiCardWrapper::MC_CmpBufStop(unsigned short channelMask) noexcept {
    return mockMulticard_->MC_CmpBufStop(channelMask);
}

int MockMultiCardWrapper::MC_CmpBufSetChannel(short nBuf1ChannelNum, short nBuf2ChannelNum) noexcept {
    return mockMulticard_->MC_CmpBufSetChannel(nBuf1ChannelNum, nBuf2ChannelNum);
}

int MockMultiCardWrapper::MC_CmpBufRpt(short nEncNum,
                                       short nDir,
                                       short nEncFlag,
                                       long lTrigValue,
                                       short nCmpNum,
                                       unsigned long lIntervalTime,
                                       short nTime,
                                       short nTimeFlag,
                                       unsigned long ulRptTime) noexcept {
    return mockMulticard_->MC_CmpBufRpt(
        nEncNum, nDir, nEncFlag, lTrigValue, nCmpNum, lIntervalTime, nTime, nTimeFlag, ulRptTime);
}

// ========== I/O 控制 ==========

int MockMultiCardWrapper::MC_SetExtDoBit(short nCardIndex, short nBitIndex, unsigned short nValue) noexcept {
    return mockMulticard_->MC_SetExtDoBit(nCardIndex, nBitIndex, nValue);
}

int MockMultiCardWrapper::MC_GetExtDoValue(short nCardIndex, unsigned long* pValue) noexcept {
    return mockMulticard_->MC_GetExtDoValue(nCardIndex, pValue);
}

int MockMultiCardWrapper::MC_GetDiRaw(short nCardIndex, long* pValue) noexcept {
    return mockMulticard_->MC_GetDiRaw(nCardIndex, pValue);
}

int MockMultiCardWrapper::MC_CrdStatus(short nCrdNum, short* pCrdStatus, long* pSegment, short FifoIndex) noexcept {
    return mockMulticard_->MC_CrdStatus(nCrdNum, pCrdStatus, pSegment, FifoIndex);
}

int MockMultiCardWrapper::MC_GetCrdPos(short nCrdNum, double* pPos) noexcept {
    return mockMulticard_->MC_GetCrdPos(nCrdNum, pPos);
}

int MockMultiCardWrapper::MC_GetCrdVel(short nCrdNum, double* pSynVel) noexcept {
    return mockMulticard_->MC_GetCrdVel(nCrdNum, pSynVel);
}

int MockMultiCardWrapper::MC_CmpBufData(short nCmpEncodeNum,
                                        short nPluseType,
                                        short nStartLevel,
                                        short nTime,
                                        long* pBuf1,
                                        short nBufLen1,
                                        long* pBuf2,
                                        short nBufLen2,
                                        short nAbsPosFlag,
                                        short nTimerFlag) noexcept {
    return mockMulticard_->MC_CmpBufData(
        nCmpEncodeNum, nPluseType, nStartLevel, nTime, pBuf1, nBufLen1, pBuf2, nBufLen2, nAbsPosFlag, nTimerFlag);
}

int MockMultiCardWrapper::MC_CmpBufSts(unsigned short* pStatus,
                                       unsigned short* pRemainData1,
                                       unsigned short* pRemainData2,
                                       unsigned short* pRemainSpace1,
                                       unsigned short* pRemainSpace2) noexcept {
    return mockMulticard_->MC_CmpBufSts(pStatus, pRemainData1, pRemainData2, pRemainSpace1, pRemainSpace2);
}

// ========== 限位控制 ==========

int MockMultiCardWrapper::MC_SetHardLimP(short axis, short type, short cardIndex, short ioIndex) noexcept {
    // Mock 实现：简单返回成功
    (void)axis;
    (void)type;
    (void)cardIndex;
    (void)ioIndex;
    return 0;
}

int MockMultiCardWrapper::MC_SetHardLimN(short axis, short type, short cardIndex, short ioIndex) noexcept {
    // Mock 实现：简单返回成功
    (void)axis;
    (void)type;
    (void)cardIndex;
    (void)ioIndex;
    return 0;
}

int MockMultiCardWrapper::MC_LmtsOn(short axis, short limitType) noexcept {
    // Mock 实现：简单返回成功
    (void)axis;
    (void)limitType;
    return 0;
}

int MockMultiCardWrapper::MC_LmtSns(unsigned short sense) noexcept {
    (void)sense;
    return 0;
}

int MockMultiCardWrapper::MC_LmtsOff(short axis, short limitType) noexcept {
    // Mock 实现：简单返回成功
    (void)axis;
    (void)limitType;
    return 0;
}

int MockMultiCardWrapper::MC_GetLmtsOnOff(short axis, short* posLimitEnabled, short* negLimitEnabled) noexcept {
    // Mock 实现：模拟限位已启用
    (void)axis;
    if (posLimitEnabled) *posLimitEnabled = 1;
    if (negLimitEnabled) *negLimitEnabled = 1;
    return 0;
}

int MockMultiCardWrapper::MC_SetLmtSnsSingle(short axis, short posPolarity, short negPolarity) noexcept {
    // Mock 实现：简单返回成功
    (void)axis;
    (void)posPolarity;
    (void)negPolarity;
    return 0;
}

int MockMultiCardWrapper::MC_SetSoftLimit(short cardIndex, short axis, long posLimit, long negLimit) noexcept {
    // Mock 实现：简单返回成功
    (void)cardIndex;
    (void)axis;
    (void)posLimit;
    (void)negLimit;
    return 0;
}

// ========== 编码器控制 ==========

int MockMultiCardWrapper::MC_EncOn(short axis) noexcept {
    return mockMulticard_->MC_EncOn(axis);
}

int MockMultiCardWrapper::MC_EncOff(short axis) noexcept {
    return mockMulticard_->MC_EncOff(axis);
}

// ========== 调试支持 ==========

void MockMultiCardWrapper::MC_StartDebugLog(int level) noexcept {
    mockMulticard_->MC_StartDebugLog(level);
}

void MockMultiCardWrapper::SetDigitalInputRaw(short card_index, long value) noexcept {
    mockMulticard_->SetDigitalInputRaw(card_index, value);
}

}  // namespace Hardware
}  // namespace Infrastructure
}  // namespace Siligen

