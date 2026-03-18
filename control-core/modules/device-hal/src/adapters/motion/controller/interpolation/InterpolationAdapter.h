#pragma once

#include "domain/motion/ports/IInterpolationPort.h"
#include "modules/device-hal/src/drivers/multicard/IMultiCardWrapper.h"
#include "shared/types/Result.h"

#include <memory>

namespace Siligen::Infrastructure::Adapters {

using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;

/**
 * @brief 插补适配器
 *
 * 实现 IInterpolationPort 接口
 * - 包装 IMultiCardWrapper 的坐标系插补功能
 * - 数据转换：InterpolationData → MultiCard API 参数
 * - 错误处理：MultiCard 错误码 → Result<T>
 *
 * 架构合规性:
 * - 实现 Domain 层端口接口
 * - 依赖注入到 Application 层
 * - 使用 Result<T> 错误处理
 */
class InterpolationAdapter : public Siligen::Domain::Motion::Ports::IInterpolationPort {
   public:
    /**
     * @brief 构造函数
     * @param wrapper MultiCard硬件包装器
     */
    explicit InterpolationAdapter(std::shared_ptr<Infrastructure::Hardware::IMultiCardWrapper> wrapper);

    ~InterpolationAdapter() override = default;

    // 禁止拷贝和移动
    InterpolationAdapter(const InterpolationAdapter&) = delete;
    InterpolationAdapter& operator=(const InterpolationAdapter&) = delete;
    InterpolationAdapter(InterpolationAdapter&&) = delete;
    InterpolationAdapter& operator=(InterpolationAdapter&&) = delete;

    // 实现 IInterpolationPort 接口
    Result<void> ConfigureCoordinateSystem(int16 coord_sys,
                                           const Siligen::Domain::Motion::Ports::CoordinateSystemConfig& config) override;

    Result<void> AddInterpolationData(int16 coord_sys,
                                      const Siligen::Domain::Motion::Ports::InterpolationData& data) override;

    Result<void> ClearInterpolationBuffer(int16 coord_sys) override;

    Result<void> FlushInterpolationData(int16 coord_sys) override;

    Result<void> StartCoordinateSystemMotion(uint32 coord_sys_mask) override;

    Result<void> StopCoordinateSystemMotion(uint32 coord_sys_mask) override;

    Result<void> SetCoordinateSystemVelocityOverride(int16 coord_sys, float32 override_percent) override;

    Result<void> EnableCoordinateSystemSCurve(int16 coord_sys, float32 jerk) override;

    Result<void> DisableCoordinateSystemSCurve(int16 coord_sys) override;

    Result<void> SetConstLinearVelocityMode(int16 coord_sys, bool enabled, uint32 rotate_axis_mask) override;

    Result<uint32> GetInterpolationBufferSpace(int16 coord_sys) const override;

    Result<uint32> GetLookAheadBufferSpace(int16 coord_sys) const override;

    Result<Siligen::Domain::Motion::Ports::CoordinateSystemStatus> GetCoordinateSystemStatus(int16 coord_sys) const override;

   private:
    std::shared_ptr<Infrastructure::Hardware::IMultiCardWrapper> wrapper_;

    /**
     * @brief 转换 MultiCard 错误码为 Result
     * @param error_code MultiCard API 返回的错误码
     * @param operation 操作描述
     */
    Result<void> ConvertError(int error_code, const std::string& operation);

    /**
     * @brief 将脉冲转换为物理单位（简化实现：1:1映射）
     * TODO: 后续可引入 UnitConverter
     */
    static constexpr double PULSE_PER_MM = 200.0;  // 脉冲当量 (与 machine_config.ini 保持一致)
};

}  // namespace Siligen::Infrastructure::Adapters







