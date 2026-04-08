#pragma once

#include "process_planning/contracts/configuration/IConfigurationPort.h"
#include "domain/motion/domain-services/HomingProcess.h"
#include "domain/motion/ports/IHomingPort.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "domain/supervision/ports/IEventPublisherPort.h"
#include "domain/motion/ports/IMotionConnectionPort.h"
#include "shared/types/AxisTypes.h"
#include "shared/types/Result.h"

#include <memory>
#include <string>
#include <vector>

namespace Siligen::Application::UseCases::Motion::Homing {

using HomeAxesRequest = Domain::Motion::DomainServices::HomeAxesRequest;
using HomeAxesResponse = Domain::Motion::DomainServices::HomeAxesResponse;

/**
 * @brief 轴回零用例
 *
 * 业务流程:
 * 1. 验证请求参数
 * 2. 检查系统状态是否允许回零
 * 3. 使用已加载配置（轴数量/范围）
 * 4. 执行回零操作
 * 5. 等待回零完成(可选)
 * 6. 验证回零状态
 * 7. 记录回零结果
 */
class HomeAxesUseCase {
   public:
    explicit HomeAxesUseCase(std::shared_ptr<Domain::Motion::Ports::IHomingPort> homing_port,
                             std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port,
                             std::shared_ptr<Domain::Motion::Ports::IMotionConnectionPort> motion_connection_port,
                             std::shared_ptr<Domain::System::Ports::IEventPublisherPort> event_port,
                             std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port = nullptr);

    ~HomeAxesUseCase() = default;

    HomeAxesUseCase(const HomeAxesUseCase&) = delete;
    HomeAxesUseCase& operator=(const HomeAxesUseCase&) = delete;
    HomeAxesUseCase(HomeAxesUseCase&&) = delete;
    HomeAxesUseCase& operator=(HomeAxesUseCase&&) = delete;

    /**
     * @brief 执行回零操作
     */
    Result<HomeAxesResponse> Execute(const HomeAxesRequest& request);

    /**
     * @brief 停止回零操作
     */
    Result<void> StopHoming();

    /**
     * @brief 停止指定轴的回零操作
     * @param axes 需要停止的轴列表(空列表表示所有轴)
     */
    Result<void> StopHomingAxes(const std::vector<Siligen::Shared::Types::LogicalAxisId>& axes);

    /**
     * @brief 检查指定轴是否已回零
     */
    Result<bool> IsAxisHomed(Siligen::Shared::Types::LogicalAxisId axis) const;

   private:
    std::shared_ptr<Domain::Motion::DomainServices::HomingProcess> homing_process_;
    std::shared_ptr<Domain::System::Ports::IEventPublisherPort> event_port_;
    void PublishHomingEvent(Siligen::Shared::Types::LogicalAxisId axis, bool success);
};

}  // namespace Siligen::Application::UseCases::Motion::Homing






