#pragma once

#include "IMultiCardWrapper.h"
#include "MockMultiCard.h"
#include "MultiCardCPP.h"  // 需要访问 TJogPrm 和 TAxisHomePrm 类型定义

#include <memory>

namespace Siligen {
namespace Infrastructure {
namespace Hardware {

/**
 * @brief Mock MultiCard 硬件包装器
 *
 * 包装 MockMultiCard，提供 IMultiCardWrapper 接口实现。
 *
 * 设计原则:
 * - 直接委托给 MockMultiCard 对象
 * - 支持单元测试和集成测试
 * - 模拟硬件行为，无需真实硬件
 */
class MockMultiCardWrapper : public IMultiCardWrapper {
   public:
    /**
     * @brief 构造函数
     * @param mockMulticard 已存在的 MockMultiCard 对象（可选）
     *
     * 如果未提供 mockMulticard，则构造函数会创建新实例。
     */
    explicit MockMultiCardWrapper(std::shared_ptr<MockMultiCard> mockMulticard = nullptr);

    /**
     * @brief 析构函数
     */
    ~MockMultiCardWrapper() override = default;

    // 禁止拷贝和移动
    MockMultiCardWrapper(const MockMultiCardWrapper&) = delete;
    MockMultiCardWrapper& operator=(const MockMultiCardWrapper&) = delete;
    MockMultiCardWrapper(MockMultiCardWrapper&&) = delete;
    MockMultiCardWrapper& operator=(MockMultiCardWrapper&&) = delete;

    // ========== IMultiCardWrapper 接口实现 ==========

    // 连接管理
    int MC_Open(short nCardNum,
                char* cPCEthernetIP,
                unsigned short nPCEthernetPort,
                char* cCardEthernetIP,
                unsigned short nCardEthernetPort) noexcept override;
    int MC_Close() noexcept override;
    int MC_Reset() noexcept override;
    int MC_ResetAllM() noexcept override;

    // 轴控制
    int MC_AxisOn(short axis) noexcept override;
    int MC_AxisOff(short axis) noexcept override;
    int MC_SetPos(short axis, long position) noexcept override;
    int MC_GetPos(short axis, long* position) noexcept override;
    int MC_GetSts(short axis, long* status, short from_axis = 1, unsigned long* clock = nullptr) noexcept override;
    int MC_GetPrfPos(short axis, double* pos) noexcept override;
    int MC_GetAxisEncPos(short axis, double* pos) noexcept override;
    int MC_GetAxisPrfVel(short axis, double* vel, short count = 1, unsigned long* clock = nullptr) noexcept override;
    int MC_GetAxisEncVel(short axis, double* vel, short count = 1, unsigned long* clock = nullptr) noexcept override;
    int MC_GetAlarmOnOff(short axis, short* alarmOnOff) noexcept override;

    // 运动控制
    int MC_SetVel(short axis, double velocity) noexcept override;
    int MC_Update(long mask) noexcept override;
    int MC_Stop(long mask, short mode) noexcept override;
    int MC_StopEx(long mask, short stopMode) noexcept override;

    // 点动控制
    int MC_PrfJog(short axis) noexcept override;
    int MC_SetJogPrm(short axis, void* jogPrm) noexcept override;

    // 回零控制
    int MC_HomeSetPrm(short axis, void* homePrm) noexcept override;
    int MC_HomeSns(unsigned short sense) noexcept override;
    int MC_GetHomeSns(unsigned short* sense) noexcept override;
    int MC_HomeStart(short axis) noexcept override;
    int MC_HomeGetSts(short axis, unsigned short* status) noexcept override;
    int MC_HomeStop(short axis) noexcept override;
    int MC_ClrSts(short axis, short count = 1) noexcept override;

    // 轮廓运动
    int MC_PrfTrap(short axis) noexcept override;
    int MC_SetTrapPrm(short axis, void* trapPrm) noexcept override;

    // 坐标系运动
    int MC_CrdSpace(short crd, long* space, short fifo = 0) noexcept override;
    int MC_CrdClear(short crd, short fifo) noexcept override;
    int MC_SetCrdPrm(short nCrdNum, short dimension, short* profile,
                      double synVelMax, double synAccMax) noexcept override;
    int MC_LnXY(short crd,
                long x,
                long y,
                double synVel,
                double synAcc,
                double endVel,
                short fifo,
                long segment_id = 0) noexcept override;
    int MC_ArcXYC(short crd,
                  long x,
                  long y,
                  double cx,
                  double cy,
                  short direction,
                  double synVel,
                  double synAcc,
                  double endVel,
                  short fifo,
                  long segment_id = 0) noexcept override;
    int MC_CrdData(short nCrdNum, void* pCrdData, short fifoIndex = 0) noexcept override;
    int MC_CrdStart(short crd, short mask) noexcept override;
    int MC_InitLookAhead(short nCrdNum,
                          short fifoIndex,
                          int lookAheadNum,
                          double* speedMax,
                          double* accMax,
                          double* maxStepSpeed,
                          double* scale) noexcept override;
    int MC_CrdSMoveEnable(short nCrdNum, double dJ) noexcept override;
    int MC_CrdSMoveDisable(short nCrdNum) noexcept override;
    int MC_SetConstLinearVelFlag(short nCrdNum,
                                 short nFifoIndex,
                                 short nConstLinearVelFlag,
                                 long lRotateAxisMask) noexcept override;
    int MC_GetLookAheadSpace(short nCrdNum, long* pSpace, short FifoIndex = 0) noexcept override;
    int MC_SetOverride(short nCrdNum, double synVelRatio) noexcept override;
    int MC_G00SetOverride(short nCrdNum, double synVelRatio) noexcept override;

    // CMP 触发
    int MC_CmpPluse(short nChannelMask,
                    short nPluseType1,
                    short nPluseType2,
                    short nTime1,
                    short nTime2,
                    short nTimeFlag1,
                    short nTimeFlag2) noexcept override;
    int MC_CmpRpt(short nCmpNum,
                  unsigned long lIntervalTime,
                  short nTime,
                  short nTimeFlag,
                  unsigned long ulRptTime) noexcept override;
    int MC_CmpBufStop(unsigned short channelMask) noexcept override;
    int MC_CmpBufSetChannel(short nBuf1ChannelNum, short nBuf2ChannelNum) noexcept override;
    int MC_CmpBufRpt(short nEncNum,
                     short nDir,
                     short nEncFlag,
                     long lTrigValue,
                     short nCmpNum,
                     unsigned long lIntervalTime,
                     short nTime,
                     short nTimeFlag,
                     unsigned long ulRptTime) noexcept override;
    int MC_CrdStatus(short nCrdNum, short* pCrdStatus, long* pSegment, short FifoIndex = 0) noexcept override;
    int MC_GetCrdPos(short nCrdNum, double* pPos) noexcept override;
    int MC_GetCrdVel(short nCrdNum, double* pSynVel) noexcept override;
    int MC_CmpBufData(short nCmpEncodeNum,
                      short nPluseType,
                      short nStartLevel,
                      short nTime,
                      long* pBuf1,
                      short nBufLen1,
                      long* pBuf2,
                      short nBufLen2,
                      short nAbsPosFlag = 0,
                      short nTimerFlag = 0) noexcept override;
    int MC_CmpBufSts(unsigned short* pStatus,
                     unsigned short* pRemainData1,
                     unsigned short* pRemainData2,
                     unsigned short* pRemainSpace1,
                     unsigned short* pRemainSpace2) noexcept override;

    // I/O 控制
    int MC_SetExtDoBit(short nCardIndex, short nBitIndex, unsigned short nValue) noexcept override;
    int MC_GetExtDoValue(short nCardIndex, unsigned long* pValue) noexcept override;
    int MC_GetDiRaw(short nCardIndex, long* pValue) noexcept override;

    // 限位控制
    int MC_SetHardLimP(short axis, short type, short cardIndex, short ioIndex) noexcept override;
    int MC_SetHardLimN(short axis, short type, short cardIndex, short ioIndex) noexcept override;
    int MC_LmtsOn(short axis, short limitType = -1) noexcept override;
    int MC_LmtSns(unsigned short sense) noexcept override;
    int MC_LmtsOff(short axis, short limitType = -1) noexcept override;
    int MC_GetLmtsOnOff(short axis, short* posLimitEnabled, short* negLimitEnabled) noexcept override;
    int MC_SetLmtSnsSingle(short axis, short posPolarity, short negPolarity) noexcept override;
    int MC_SetSoftLimit(short cardIndex, short axis, long posLimit, long negLimit) noexcept override;

    // 编码器控制
    int MC_EncOn(short axis) noexcept override;
    int MC_EncOff(short axis) noexcept override;

    // 调试支持
    void MC_StartDebugLog(int level) noexcept override;

    // Mock-only 测试控制
    void SetDigitalInputRaw(short card_index, long value) noexcept;
    void SimulateDisconnect() noexcept;
    void ResetConnectionSimulation() noexcept;

   private:
    std::shared_ptr<MockMultiCard> mockMulticard_;  ///< Mock MultiCard 对象
};

}  // namespace Hardware
}  // namespace Infrastructure
}  // namespace Siligen
