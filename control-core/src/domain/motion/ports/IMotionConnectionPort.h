#pragma once

#include "shared/types/Result.h"

#include <string>

// 导入共享类型
using Siligen::Shared::Types::Result;

// 标准类型定义
using int16 = std::int16_t;

namespace Siligen::Domain::Motion::Ports {

/**
 * @brief 运动控制器连接管理端口接口
 * 定义运动控制器的连接、断开和基础管理操作
 * 符合接口隔离原则(ISP) - 客户端仅依赖连接管理功能
 */
class IMotionConnectionPort {
   public:
    virtual ~IMotionConnectionPort() = default;

    /**
     * @brief 连接运动控制卡
     * @param card_ip 控制卡IP地址
     * @param pc_ip PC IP地址
     * @param port 端口号
     * @return 连接结果
     */
    virtual Result<void> Connect(const std::string& card_ip, const std::string& pc_ip, int16 port = 0) = 0;

    /**
     * @brief 断开连接
     */
    virtual Result<void> Disconnect() = 0;

    /**
     * @brief 检查连接状态
     */
    virtual Result<bool> IsConnected() const = 0;

    /**
     * @brief 复位控制卡
     */
    virtual Result<void> Reset() = 0;

    /**
     * @brief 获取控制卡信息
     */
    virtual Result<std::string> GetCardInfo() const = 0;
};

}  // namespace Siligen::Domain::Motion::Ports

