// MultiCardStub_Class.cpp - MultiCard C API的存根实现
// 仅提供C API存根，MultiCard C++类由厂商MultiCard.lib提供

// 重要说明：
// - MultiCard C++类（构造函数、成员函数）由厂商MultiCard.lib提供真实实现
// - 本文件仅提供纯C API存根，供HardwareTestAdapter等测试代码使用
// - 厂商SDK中MC_PrfJog是类成员函数，而测试代码需要纯C函数

#include "siligen/device/adapters/drivers/multicard/MultiCardCPP.h"

#include <stdio.h>

// ============================================
// C API存根实现（用于HardwareTestAdapter等）
// 这些是纯C函数，与厂商SDK的C++类成员函数不同
// ============================================

#ifdef __cplusplus
extern "C" {
#endif

short MC_PrfJog(short cardNum, short axis) {
    fprintf(stderr, "[MultiCard Stub] MC_PrfJog stub (C API)\n");
    return -999;
}

short MC_SetVel(short cardNum, short axis, double vel) {
    return -999;
}

short MC_Update(short cardNum, unsigned long mask) {
    return -999;
}

short MC_Stop(short cardNum, unsigned long mask, unsigned long smoothStop) {
    return -999;
}

short MC_GetPrfPos(short cardNum, short axis, double* pos) {
    if (pos) *pos = 0.0;
    return -999;
}

short MC_GetSts(short cardNum, short axis, long* pSts, short count, unsigned long* pClock) {
    if (pSts) *pSts = 0;
    if (pClock) *pClock = 0;
    return -999;
}

short MC_SetExtDoBit(short cardNum, short doType, short index, short value) {
    return -999;
}

short MC_GetDiRaw(short cardNum, short diType, long* value) {
    if (value) *value = 0;
    return -999;
}

short MC_GetExtDoValue(short cardNum, short doType, long* pValue) {
    if (pValue) *pValue = 0;
    return -999;
}

short MC_HomeSetPrm(short cardNum, short axis, TAxisHomePrm* pHomePrm) {
    return -999;
}

short MC_HomeStart(short cardNum, short axis) {
    return -999;
}

short MC_HomeStop(short cardNum, short axis) {
    return -999;
}

short MC_CmpBufData(short nCmpEncodeNum,
                    short nPluseType,
                    short nStartLevel,
                    short nTime,
                    long* pBuf1,
                    short nBufLen1,
                    long* pBuf2,
                    short nBufLen2,
                    short nAbsPosFlag,
                    short nTimerFlag) {
    return -999;
}

short MC_CrdClear(short cardNum, short crd) {
    return -999;
}

short MC_LnXY(short cardNum,
              long x_pulse,
              long y_pulse,
              double synVel,
              double synAcc,
              double endVel,
              short fifo,
              long segmentNum) {
    return -999;
}

short MC_ArcXYC(short cardNum,
                short crd,
                long x,
                long y,
                double xCenter,
                double yCenter,
                short circleDir,
                double synVel,
                double synAcc,
                double endVel,
                short fifo,
                long segmentNum) {
    return -999;
}

short MC_GetCrdStatus(short crd, short* pnCrdStatus, short* pnFifoStatus) {
    if (pnCrdStatus) *pnCrdStatus = 0;
    if (pnFifoStatus) *pnFifoStatus = 0;
    return -999;
}

#ifdef __cplusplus
}
#endif

// ============================================
// MultiCard C++类存根实现（当厂商lib不存在时）
// ============================================

int MultiCard::MC_PrfTrap(short axis) {
    return -999;
}

int MultiCard::MC_SetPos(short axis, long pos) {
    return -999;
}

int MultiCard::MC_SetVel(short axis, double vel) {
    return -999;
}

int MultiCard::MC_Update(long mask) {
    return -999;
}

int MultiCard::MC_GetPrfPos(short axis, double* pos, short count, unsigned long* pClock) {
    if (pos) *pos = 0.0;
    if (pClock) *pClock = 0;
    return -999;
}

int MultiCard::MC_GetAxisPrfPos(short axis, double* pos, short count, unsigned long* pClock) {
    if (pos) *pos = 0.0;
    if (pClock) *pClock = 0;
    return -999;
}

int MultiCard::MC_GetAxisPrfVel(short axis, double* vel, short count, unsigned long* pClock) {
    if (vel) *vel = 0.0;
    if (pClock) *pClock = 0;
    return -999;
}

int MultiCard::MC_GetSts(short axis, long* pSts, short count, unsigned long* pClock) {
    if (pSts) *pSts = 0;
    if (pClock) *pClock = 0;
    return -999;
}

int MultiCard::MC_Stop(long mask, long smoothStop) {
    return -999;
}

int MultiCard::MC_StopEx(long crdMask, long crdOption, long axisMask0, long axisOption0) {
    (void)crdMask;
    (void)crdOption;
    (void)axisMask0;
    (void)axisOption0;
    return -999;
}

int MultiCard::MC_CrdSMoveEnable(short nCrdNum, double dJ) {
    (void)nCrdNum;
    (void)dJ;
    return -999;
}

int MultiCard::MC_CrdSMoveDisable(short nCrdNum) {
    (void)nCrdNum;
    return -999;
}

int MultiCard::MC_SetConstLinearVelFlag(short nCrdNum,
                                        short nFifoIndex,
                                        short nConstLinearVelFlag,
                                        long lRotateAxisMask) {
    (void)nCrdNum;
    (void)nFifoIndex;
    (void)nConstLinearVelFlag;
    (void)lRotateAxisMask;
    return -999;
}

int MultiCard::MC_GetLookAheadSpace(short nCrdNum, long* pSpace, short FifoIndex) {
    (void)nCrdNum;
    (void)FifoIndex;
    if (pSpace) {
        *pSpace = 0;
    }
    return -999;
}

int MultiCard::MC_SetOverride(short nCrdNum, double synVelRatio) {
    (void)nCrdNum;
    (void)synVelRatio;
    return -999;
}

int MultiCard::MC_G00SetOverride(short nCrdNum, double synVelRatio) {
    (void)nCrdNum;
    (void)synVelRatio;
    return -999;
}

int MultiCard::MC_SetTrapPrmSingle(short axis, double acc, double dec, double smoothTime, short profile) {
    return -999;
}

int MultiCard::MC_SetSoftLimit(short axis, long posLimit, long negLimit) {
    return -999;
}

int MultiCard::MC_SetHardLimP(short axis, short enable, short mode, short source) {
    return -999;
}

int MultiCard::MC_SetHardLimN(short axis, short enable, short mode, short source) {
    return -999;
}

int MultiCard::MC_LmtsOn(short axis, short limitType) {
    return -999;
}

int MultiCard::MC_LmtsOff(short axis, short limitType) {
    return -999;
}

int MultiCard::MC_SetLmtSnsSingle(short axis, short limitType, short sense) {
    return -999;
}

