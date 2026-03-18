#pragma once

#include "domain/safety/value-objects/InterlockTypes.h"
#include "shared/types/Result.h"

namespace Siligen {
namespace Domain {
namespace Safety {
namespace Ports {

using Shared::Types::Result;
using ValueObjects::InterlockSignals;

/**
 * @brief 互锁信号端口
 * 由基础设施层提供实现，Domain 通过端口获取互锁信号快照。
 */
class IInterlockSignalPort {
   public:
    virtual ~IInterlockSignalPort() = default;

    /**
     * @brief 读取当前互锁信号快照
     * @return Result<InterlockSignals> 成功返回信号快照，失败返回错误
     */
    virtual Result<InterlockSignals> ReadSignals() const noexcept = 0;
};

}  // namespace Ports
}  // namespace Safety
}  // namespace Domain
}  // namespace Siligen
