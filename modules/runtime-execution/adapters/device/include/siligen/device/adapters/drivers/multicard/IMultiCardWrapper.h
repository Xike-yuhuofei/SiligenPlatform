#pragma once

#include <cstdint>
#include <memory>

namespace Siligen {
namespace Infrastructure {
namespace Hardware {

/**
 * @brief MultiCard 硬件包装器接口
 *
 * 提供真正的多态性接口，封装 MultiCard SDK 和 MockMultiCard。
 *
 * 设计原则:
 * - 虚函数接口支持运行时多态
 * - 完整包装所有适配器使用的 MultiCard API
 * - 错误码保持与 MultiCard SDK 一致（返回 int，0=成功）
 * - noexcept 标记关键性能路径方法
 *
 * 命名空间: Siligen::Infrastructure::Hardware
 * 符合 HARDWARE_DOMAIN_ISOLATION 规则（Domain 层不能直接访问）
 */
class IMultiCardWrapper {
   public:
    virtual ~IMultiCardWrapper() = default;

    // ========== 连接管理 ==========

    /**
     * @brief 打开控制卡连接
     * @param nCardNum 卡号（固定为0，保留参数用于API兼容）
     * @param cPCEthernetIP 本机IP地址
     * @param nPCEthernetPort 本机端口
     * @param cCardEthernetIP 控制卡IP地址
     * @param nCardEthernetPort 控制卡端口
     * @return 错误码（0=成功，非0=失败）
     */
    virtual int MC_Open(short nCardNum,
                        char* cPCEthernetIP,
                        unsigned short nPCEthernetPort,
                        char* cCardEthernetIP,
                        unsigned short nCardEthernetPort) noexcept = 0;

    /**
     * @brief 关闭控制卡连接
     * @return 错误码（0=成功）
     */
    virtual int MC_Close() noexcept = 0;

    /**
     * @brief 复位控制卡
     * @return 错误码（0=成功）
     */
    virtual int MC_Reset() noexcept = 0;

    /**
     * @brief 复位控制卡所有模块
     * @return 错误码（0=成功）
     */
    virtual int MC_ResetAllM() noexcept = 0;

    // ========== 轴控制 ==========

    /**
     * @brief 使能轴
     * @param axis 轴号（1-based，如 1-2）
     * @return 错误码（0=成功）
     */
    virtual int MC_AxisOn(short axis) noexcept = 0;

    /**
     * @brief 失能轴
     * @param axis 轴号（1-based）
     * @return 错误码（0=成功）
     */
    virtual int MC_AxisOff(short axis) noexcept = 0;

    /**
     * @brief 设置轴当前位置
     * @param axis 轴号（1-based）
     * @param position 目标位置（脉冲）
     * @return 错误码（0=成功）
     */
    virtual int MC_SetPos(short axis, long position) noexcept = 0;

    /**
     * @brief 获取轴当前位置
     * @param axis 轴号（1-based）
     * @param position 输出位置指针（脉冲）
     * @return 错误码（0=成功）
     */
    virtual int MC_GetPos(short axis, long* position) noexcept = 0;

    /**
     * @brief 获取轴状态
     * @param axis 轴号（1-based）
     * @param status 输出状态位指针
     * @param from_axis 源轴号（默认1）
     * @param clock 输出时钟指针（可选，默认nullptr）
     * @return 错误码（0=成功）
     */
    virtual int MC_GetSts(short axis, long* status, short from_axis = 1, unsigned long* clock = nullptr) noexcept = 0;

    /**
     * @brief 获取轴轮廓位置
     * @param axis 轴号（1-based）
     * @param pos 输出位置指针（双精度）
     * @return 错误码（0=成功）
     */
    virtual int MC_GetPrfPos(short axis, double* pos) noexcept = 0;

    /**
     * @brief 获取轴编码器位置
     * @param axis 轴号（1-based）
     * @param pos 输出位置指针（双精度）
     * @return 错误码（0=成功）
     */
    virtual int MC_GetAxisEncPos(short axis, double* pos) noexcept = 0;

    /**
     * @brief 获取轴轮廓速度
     * @param axis 轴号（1-based）
     * @param vel 输出速度指针（双精度，脉冲/毫秒）
     * @param count 轴数量（默认1）
     * @param clock 输出时钟指针（可选，默认nullptr）
     * @return 错误码（0=成功）
     */
    virtual int MC_GetAxisPrfVel(short axis, double* vel, short count = 1, unsigned long* clock = nullptr) noexcept = 0;

    /**
     * @brief 获取轴编码器速度
     * @param axis 轴号（1-based）
     * @param vel 输出速度指针（双精度，脉冲/毫秒）
     * @param count 轴数量（默认1）
     * @param clock 输出时钟指针（可选，默认nullptr）
     * @return 错误码（0=成功）
     */
    virtual int MC_GetAxisEncVel(short axis, double* vel, short count = 1, unsigned long* clock = nullptr) noexcept = 0;

    /**
     * @brief 获取轴报警状态
     * @param axis 轴号（1-based）
     * @param alarmOnOff 输出报警状态（0=无报警，1=报警）
     * @return 错误码（0=成功）
     */
    virtual int MC_GetAlarmOnOff(short axis, short* alarmOnOff) noexcept = 0;

    // ========== 运动控制 ==========

    /**
     * @brief 设置轴速度
     * @param axis 轴号（1-based）
     * @param velocity 速度（脉冲/秒）
     * @return 错误码（0=成功）
     */
    virtual int MC_SetVel(short axis, double velocity) noexcept = 0;

    /**
     * @brief 更新轴运动
     * @param mask 轴掩码（如 0x01=轴0, 0x02=轴1）
     * @return 错误码（0=成功）
     */
    virtual int MC_Update(long mask) noexcept = 0;

    /**
     * @brief 停止轴运动
     * @param mask 轴掩码
     * @param mode 停止模式（0=减速停止，1=立即停止）
     * @return 错误码（0=成功）
     */
    virtual int MC_Stop(long mask, short mode) noexcept = 0;

    // ========== 点动控制 ==========

    /**
     * @brief 设置点动模式
     * @param axis 轴号（1-based）
     * @return 错误码（0=成功）
     */
    virtual int MC_PrfJog(short axis) noexcept = 0;

    /**
     * @brief 设置点动参数
     * @param axis 轴号（1-based）
     * @param jogPrm 点动参数指针
     * @return 错误码（0=成功）
     */
    virtual int MC_SetJogPrm(short axis, void* jogPrm) noexcept = 0;

    // ========== 回零控制 ==========

    /**
     * @brief 设置回零参数
     * @param axis 轴号（1-based）
     * @param homePrm 回零参数指针
     * @return 错误码（0=成功）
     */
    virtual int MC_HomeSetPrm(short axis, void* homePrm) noexcept = 0;

    /**
     * @brief 设置HOME输入电平逻辑
     * @param sense HOME输入电平逻辑位掩码(bit0-bit7对应轴1-8)
     * @return 错误码（0=成功）
     */
    virtual int MC_HomeSns(unsigned short sense) noexcept = 0;

    /**
     * @brief 获取HOME输入电平逻辑
     * @param sense 输出HOME输入电平逻辑位掩码(bit0-bit7对应轴1-8)
     * @return 错误码（0=成功）
     */
    virtual int MC_GetHomeSns(unsigned short* sense) noexcept = 0;

    /**
     * @brief 启动回零
     * @param axis 轴号（1-based）
     * @return 错误码（0=成功）
     */
    virtual int MC_HomeStart(short axis) noexcept = 0;

    /**
     * @brief 获取回零状态
     * @param axis 轴号（1-based）
     * @param status 输出状态指针(0=未完成,1=成功,2=失败)
     * @return 错误码（0=成功）
     */
    virtual int MC_HomeGetSts(short axis, unsigned short* status) noexcept = 0;

    /**
     * @brief 停止回零
     * @param axis 轴号（1-based）
     * @return 错误码（0=成功）
     */
    virtual int MC_HomeStop(short axis) noexcept = 0;

    /**
     * @brief 清除轴状态和报警
     * @param axis 轴号（1-based）
     * @param count 轴数量（默认1）
     * @return 错误码（0=成功）
     */
    virtual int MC_ClrSts(short axis, short count = 1) noexcept = 0;

    // ========== 轮廓运动 ==========

    /**
     * @brief 设置梯形轮廓模式
     * @param axis 轴号（1-based）
     * @return 错误码（0=成功）
     */
    virtual int MC_PrfTrap(short axis) noexcept = 0;

    /**
     * @brief 设置梯形轮廓运动参数
     * @param axis 轴号（1-based）
     * @param trapPrm 梯形参数指针
     * @return 错误码（0=成功）
     */
    virtual int MC_SetTrapPrm(short axis, void* trapPrm) noexcept = 0;

    // ========== 坐标系运动 ==========

    /**
     * @brief 设置坐标系空间
     * @param crd 坐标系编号
     * @param space 空间指针
     * @param fifo FIFO编号（默认0）
     * @return 错误码（0=成功）
     */
    virtual int MC_CrdSpace(short crd, long* space, short fifo = 0) noexcept = 0;

    /**
     * @brief 清空坐标系缓冲区
     * @param crd 坐标系编号
     * @param fifo FIFO编号
     * @return 错误码（0=成功）
     */
    virtual int MC_CrdClear(short crd, short fifo) noexcept = 0;

    /**
     * @brief 设置坐标系参数
     * @param nCrdNum 坐标系编号（1-based）
     * @param dimension 维度（2=XY, 3=XYZ）
     * @param profile 轴映射数组（profile[0]=X轴号, profile[1]=Y轴号, ...）
     * @param synVelMax 最大合成速度（脉冲/秒）
     * @param synAccMax 最大合成加速度（脉冲/秒²）
     * @param setOriginFlag 原点模式（0=当前规划位置，1=使用 originPos）
     * @param originPos 用户指定原点位置数组（脉冲，可为 nullptr 表示全 0）
     * @return 错误码（0=成功）
     */
    virtual int MC_SetCrdPrm(short nCrdNum,
                             short dimension,
                             short* profile,
                             double synVelMax,
                             double synAccMax,
                             short setOriginFlag = 0,
                             const long* originPos = nullptr) noexcept = 0;

    /**
     * @brief 直线插补（XY平面）
     * @param crd 坐标系编号
     * @param x X轴目标位置（脉冲）
     * @param y Y轴目标位置（脉冲）
     * @param synVel 同步速度（脉冲/秒）
     * @param synAcc 同步加速度（脉冲/秒²）
     * @param endVel 结束速度（脉冲/秒）
     * @param fifo FIFO编号
     * @param segment_id 线段ID（默认0）
     * @return 错误码（0=成功）
     */
    virtual int MC_LnXY(short crd,
                        long x,
                        long y,
                        double synVel,
                        double synAcc,
                        double endVel,
                        short fifo,
                        long segment_id = 0) noexcept = 0;

    /**
     * @brief 圆弧插补（圆心模式）
     * @param crd 坐标系编号
     * @param x 终点X坐标（脉冲）
     * @param y 终点Y坐标（脉冲）
     * @param cx 圆心X坐标（脉冲）
     * @param cy 圆心Y坐标（脉冲）
     * @param direction 方向（1=顺时针，0=逆时针）
     * @param synVel 同步速度（脉冲/秒）
     * @param synAcc 同步加速度（脉冲/秒²）
     * @param endVel 结束速度（脉冲/秒）
     * @param fifo FIFO编号
     * @param segment_id 线段ID（默认0）
     * @return 错误码（0=成功）
     */
    virtual int MC_ArcXYC(short crd,
                          long x,
                          long y,
                          double cx,
                          double cy,
                          short direction,
                          double synVel,
                          double synAcc,
                          double endVel,
                          short fifo,
                          long segment_id = 0) noexcept = 0;

    /**
     * @brief 发送坐标系数据流结束标识
     * @param nCrdNum 坐标系编号（1-16）
     * @param pCrdData 坐标系数据指针（通常为nullptr表示结束）
     * @param fifoIndex FIFO索引（默认0）
     * @return 错误码（0=成功）
     */
    virtual int MC_CrdData(short nCrdNum, void* pCrdData, short fifoIndex = 0) noexcept = 0;

    /**
     * @brief 启动坐标系运动
     * @param crd 坐标系编号
     * @param mask 轴掩码
     * @return 错误码（0=成功）
     */
    virtual int MC_CrdStart(short crd, short mask) noexcept = 0;

    /**
     * @brief 初始化前瞻缓冲区
     * @param nCrdNum 坐标系编号（1-based）
     * @param fifoIndex FIFO索引（通常为0）
     * @param lookAheadNum 前瞻段数（推荐200-500）
     * @param speedMax 各轴最大速度数组（脉冲/毫秒），至少8个元素
     * @param accMax 各轴最大加速度数组（脉冲/毫秒^2），至少8个元素
     * @param maxStepSpeed 各轴最大速度变化量数组（相当于启动速度），至少8个元素
     * @param scale 各轴脉冲当量数组（通常为1.0），至少8个元素
     * @return 错误码（0=成功）
     * @note 必须在 MC_SetCrdPrm 之后、MC_CrdStart 之前调用
     *       缺少此调用会导致坐标系运动无法正常执行
     */
    virtual int MC_InitLookAhead(short nCrdNum,
                                  short fifoIndex,
                                  int lookAheadNum,
                                  double* speedMax,
                                  double* accMax,
                                  double* maxStepSpeed,
                                  double* scale) noexcept = 0;

    // ========== CMP 触发 ==========

    /**
     * @brief 设置比较器输出 IO 立即输出指定电平或脉冲
     * @param nChannelMask 通道掩码（bit0=通道1，bit1=通道2）
     * @param nPluseType1 通道1输出类型（0=低电平，1=高电平，2=脉冲）
     * @param nPluseType2 通道2输出类型（0=低电平，1=高电平，2=脉冲）
     * @param nTime1 通道1脉冲持续时间（输出类型为脉冲时有效）
     * @param nTime2 通道2脉冲持续时间（输出类型为脉冲时有效）
     * @param nTimeFlag1 通道1时间单位（0=微秒，1=毫秒）
     * @param nTimeFlag2 通道2时间单位（0=微秒，1=毫秒）
     * @return 错误码（0=成功）
     * @note 官方示例用法: MC_CmpPluse(1, 2, 0, 1000, 0, 1, 0) 输出1000ms脉冲
     */
    virtual int MC_CmpPluse(short nChannelMask,
                            short nPluseType1,
                            short nPluseType2,
                            short nTime1,
                            short nTime2,
                            short nTimeFlag1,
                            short nTimeFlag2) noexcept = 0;

    /**
     * @brief 停止 CMP 触发输出
     * @param channelMask 通道掩码
     * @return 错误码（0=成功）
     */
    virtual int MC_CmpBufStop(unsigned short channelMask) noexcept = 0;

    /**
     * @brief 设置 CMP 缓冲区通道绑定
     * @param nBuf1ChannelNum 缓冲区1绑定的 CMP 通道号（1-16，0=不使用）
     * @param nBuf2ChannelNum 缓冲区2绑定的 CMP 通道号（1-16，0=不使用）
     * @return 错误码（0=成功）
     * @note 用于配置两个 compare buffer 映射到哪个 CMP 输出通道
     */
    virtual int MC_CmpBufSetChannel(short nBuf1ChannelNum, short nBuf2ChannelNum) noexcept = 0;

    // ========== I/O 控制 ==========

    /**
     * @brief 设置扩展 DO 位
     * @param nCardIndex 卡索引（固定为0）
     * @param nBitIndex 位索引（0-15）
     * @param nValue 值（0=低电平，1=高电平）
     * @return 错误码（0=成功）
     */
    virtual int MC_SetExtDoBit(short nCardIndex, short nBitIndex, unsigned short nValue) noexcept = 0;

    /**
     * @brief 获取扩展 DO 值
     * @param nCardIndex 卡索引（固定为0）
     * @param pValue 输出值指针
     * @return 错误码（0=成功）
     */
    virtual int MC_GetExtDoValue(short nCardIndex, unsigned long* pValue) noexcept = 0;

    /**
     * @brief 获取 DI 原始值
     * @param nDiType DI类型：0=正限位, 1=负限位, 2=驱动报警, 3=原点开关, 4=通用输入, 7=手轮IO
     * @param pValue 输出值指针
     * @return 错误码（0=成功）
     */
    virtual int MC_GetDiRaw(short nDiType, long* pValue) noexcept = 0;

    // ========== 系统诊断 ==========

    /**
     * @brief 获取坐标系状态
     * @param nCrdNum 坐标系数目
     * @param pCrdStatus 坐标系状态指针
     * @param pSegment 段指针
     * @param FifoIndex FIFO索引（默认0）
     * @return 错误码（0=成功）
     */
    virtual int MC_CrdStatus(short nCrdNum, short* pCrdStatus, long* pSegment, short FifoIndex = 0) noexcept = 0;

    /**
     * @brief 获取坐标系位置
     * @param nCrdNum 坐标系编号
     * @param pPos 输出位置数组（至少8个double）
     * @return 错误码（0=成功）
     */
    virtual int MC_GetCrdPos(short nCrdNum, double* pPos) noexcept = 0;

    /**
     * @brief 获取坐标系合成速度
     * @param nCrdNum 坐标系编号
     * @param pSynVel 输出速度指针
     * @return 错误码（0=成功）
     */
    virtual int MC_GetCrdVel(short nCrdNum, double* pSynVel) noexcept = 0;

    /**
     * @brief 设置 CMP 缓冲区数据
     * @param nCmpEncodeNum 编码器号
     * @param nPluseType 脉冲类型
     * @param nStartLevel 起始电平
     * @param nTime 时间
     * @param pBuf1 缓冲区1指针
     * @param nBufLen1 缓冲区1长度
     * @param pBuf2 缓冲区2指针（可为NULL）
     * @param nBufLen2 缓冲区2长度
     * @param nAbsPosFlag 绝对位置标志（默认0）
     * @param nTimerFlag 定时器标志（默认0）
     * @return 错误码（0=成功）
     */
    virtual int MC_CmpBufData(short nCmpEncodeNum,
                              short nPluseType,
                              short nStartLevel,
                              short nTime,
                              long* pBuf1,
                              short nBufLen1,
                              long* pBuf2,
                              short nBufLen2,
                              short nAbsPosFlag = 0,
                              short nTimerFlag = 0) noexcept = 0;

    /**
     * @brief 获取 CMP 缓冲区状态与剩余数据
     * @param pStatus 状态字
     * @param pRemainData1 缓冲区1剩余数据
     * @param pRemainData2 缓冲区2剩余数据
     * @param pRemainSpace1 缓冲区1剩余空间
     * @param pRemainSpace2 缓冲区2剩余空间
     * @return 错误码（0=成功）
     */
    virtual int MC_CmpBufSts(unsigned short* pStatus,
                             unsigned short* pRemainData1,
                             unsigned short* pRemainData2,
                             unsigned short* pRemainSpace1,
                             unsigned short* pRemainSpace2) noexcept = 0;

    // ========== 限位控制 ==========

    /**
     * @brief 设置正向硬限位 IO 映射
     * @param axis 轴号（0-based）
     * @param type 信号类型（0=限位信号，3=HOME信号）
     * @param cardIndex 卡索引（默认0）
     * @param ioIndex IO 索引
     * @return 错误码（0=成功）
     */
    virtual int MC_SetHardLimP(short axis, short type, short cardIndex, short ioIndex) noexcept = 0;

    /**
     * @brief 设置负向硬限位 IO 映射
     * @param axis 轴号（0-based）
     * @param type 信号类型（0=限位信号，3=HOME信号）
     * @param cardIndex 卡索引（默认0）
     * @param ioIndex IO 索引
     * @return 错误码（0=成功）
     */
    virtual int MC_SetHardLimN(short axis, short type, short cardIndex, short ioIndex) noexcept = 0;

    /**
     * @brief 启用限位功能
     * @param axis 轴号（0-based）
     * @param limitType 限位类型（-1=正负向都启用，0=仅正向，1=仅负向）
     * @return 错误码（0=成功）
     */
    virtual int MC_LmtsOn(short axis, short limitType = -1) noexcept = 0;

    /**
     * @brief 全局设置限位触发电平
     * @param sense 16位掩码，Bit0=轴1正限位，Bit1=轴1负限位，...，1=低电平触发，0=高电平触发
     * @return 错误码（0=成功）
     */
    virtual int MC_LmtSns(unsigned short sense) noexcept = 0;

    /**
     * @brief 禁用限位功能
     * @param axis 轴号（0-based）
     * @param limitType 限位类型（-1=正负向都禁用，0=仅正向，1=仅负向）
     * @return 错误码（0=成功）
     */
    virtual int MC_LmtsOff(short axis, short limitType = -1) noexcept = 0;

    /**
     * @brief 获取限位启用状态
     * @param axis 轴号（0-based）
     * @param posLimitEnabled 正向限位启用状态输出（1=启用，0=禁用）
     * @param negLimitEnabled 负向限位启用状态输出（1=启用，0=禁用）
     * @return 错误码（0=成功）
     */
    virtual int MC_GetLmtsOnOff(short axis, short* posLimitEnabled, short* negLimitEnabled) noexcept = 0;

    /**
     * @brief 设置单轴限位信号极性
     * @param axis 轴号（0-based）
     * @param posPolarity 正向限位极性（1=低电平触发，0=高电平触发）
     * @param negPolarity 负向限位极性（1=低电平触发，0=高电平触发）
     * @return 错误码（0=成功）
     * @note 参考官方文档 MC_LmtSns 位定义
     */
    virtual int MC_SetLmtSnsSingle(short axis, short posPolarity, short negPolarity) noexcept = 0;

    /**
     * @brief 设置软限位
     * @param cardIndex 卡索引（固定为1）
     * @param axis 轴号（0-based）
     * @param negLimit 负向软限位（脉冲）
     * @param posLimit 正向软限位（脉冲）
     * @return 错误码（0=成功）
     */
    // MC_SetSoftLimit 参数顺序与厂商SDK一致: (axis, positive_limit, negative_limit)
    virtual int MC_SetSoftLimit(short cardIndex, short axis, long posLimit, long negLimit) noexcept = 0;

    // ========== 编码器控制 ==========

    /**
     * @brief 开启编码器反馈（CMP 使用编码器位置比较）
     * @param axis 轴号（1-16）
     * @return 错误码（0=成功）
     */
    virtual int MC_EncOn(short axis) noexcept = 0;

    /**
     * @brief 关闭编码器反馈（CMP 使用规划位置比较）
     * @param axis 轴号（1-16）
     * @return 错误码（0=成功）
     * @note 关闭编码器后，CMP 比较源切换为规划位置（Profile Position）
     *       这样可以保证非匀速运动时触发位置的均匀性
     */
    virtual int MC_EncOff(short axis) noexcept = 0;

    // ========== 速度控制 ==========

    /**
     * @brief 停止坐标系运动（扩展版本）
     * @param mask 坐标系掩码
     * @param stopMode 停止模式（0=减速停止，1=立即停止）
     * @return 错误码（0=成功）
     */
    virtual int MC_StopEx(long mask, short stopMode) noexcept = 0;

    /**
     * @brief 设置坐标系速度倍率
     * @param nCrdNum 坐标系编号
     * @param synVelRatio 速度倍率（0.0-1.0）
     * @return 错误码（0=成功）
     */
    virtual int MC_SetOverride(short nCrdNum, double synVelRatio) noexcept = 0;

    /**
     * @brief 设置G00快速定位速度倍率
     * @param nCrdNum 坐标系编号
     * @param synVelRatio 速度倍率（0.0-1.0）
     * @return 错误码（0=成功）
     */
    virtual int MC_G00SetOverride(short nCrdNum, double synVelRatio) noexcept = 0;

    /**
     * @brief 启用S曲线加减速
     * @param nCrdNum 坐标系编号
     * @param dJ 加加速度（jerk）
     * @return 错误码（0=成功）
     */
    virtual int MC_CrdSMoveEnable(short nCrdNum, double dJ) noexcept = 0;

    /**
     * @brief 禁用S曲线加减速
     * @param nCrdNum 坐标系编号
     * @return 错误码（0=成功）
     */
    virtual int MC_CrdSMoveDisable(short nCrdNum) noexcept = 0;

    /**
     * @brief 设置恒定线速度标志
     * @param nCrdNum 坐标系编号
     * @param nFifoIndex FIFO索引
     * @param nConstLinearVelFlag 恒定线速度标志
     * @param lRotateAxisMask 旋转轴掩码
     * @return 错误码（0=成功）
     */
    virtual int MC_SetConstLinearVelFlag(short nCrdNum,
                                         short nFifoIndex,
                                         short nConstLinearVelFlag,
                                         long lRotateAxisMask) noexcept = 0;

    /**
     * @brief 获取前瞻缓冲区剩余空间
     * @param nCrdNum 坐标系编号
     * @param pSpace 输出空间指针
     * @param FifoIndex FIFO索引（默认0）
     * @return 错误码（0=成功）
     */
    virtual int MC_GetLookAheadSpace(short nCrdNum, long* pSpace, short FifoIndex = 0) noexcept = 0;

    // ========== 调试支持 ==========

    /**
     * @brief 启动调试日志
     * @param level 日志级别（0-3）
     */
    virtual void MC_StartDebugLog(int level) noexcept = 0;

    // ========== 底层访问 ==========

    /**
     * @brief 获取底层 MultiCard 对象（可选实现）
     * @return MultiCard 对象的 void 指针，如果不支持则返回 nullptr
     * @note 这是一个临时方案，用于需要直接访问 MultiCard API 的场景
     *       未来应该将所有需要的 API 添加到 IMultiCardWrapper 接口
     */
    virtual void* GetUnderlyingMultiCard() noexcept { return nullptr; }
};

}  // namespace Hardware
}  // namespace Infrastructure
}  // namespace Siligen
