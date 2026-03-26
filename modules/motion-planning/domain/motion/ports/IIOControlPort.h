#pragma once

#include "shared/types/Result.h"
#include "shared/types/Types.h"

// 导入共享类型
using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Result;

// 标准类型定义
using int16 = std::int16_t;
using uint32 = std::uint32_t;
using int64 = std::int64_t;

namespace Siligen::Domain::Motion::Ports {

/**
 * @brief IO状态
 */
struct IOStatus {
    bool signal_active = false;
    int32 channel = 0;
    uint32 value = 0;
    int64 timestamp = 0;
};

/**
 * @brief IO控制端口接口
 * 定义数字IO读写和信号查询操作
 * 符合接口隔离原则(ISP) - 客户端仅依赖IO控制功能
 */
class IIOControlPort {
   public:
    virtual ~IIOControlPort() = default;

    /**
     * @brief 读取数字输入
     * @param channel 通道号 (0-15)
     */
    virtual Result<IOStatus> ReadDigitalInput(int16 channel) = 0;

    /**
     * @brief 读取数字输出
     * @param channel 通道号 (0-15)
     */
    virtual Result<IOStatus> ReadDigitalOutput(int16 channel) = 0;

    /**
     * @brief 写入数字输出
     * @param channel 通道号 (0-15)
     * @param value 输出值
     */
    virtual Result<void> WriteDigitalOutput(int16 channel, bool value) = 0;

    /**
     * @brief 读取限位状态
     * @param axis 轴号
     * @param positive 是否正向限位
     */
    virtual Result<bool> ReadLimitStatus(LogicalAxisId axis, bool positive) = 0;

    /**
     * @brief 读取伺服报警状态
     * @param axis 轴号
     */
    virtual Result<bool> ReadServoAlarm(LogicalAxisId axis) = 0;
};

}  // namespace Siligen::Domain::Motion::Ports

