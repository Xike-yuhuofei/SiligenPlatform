#include "siligen/device/adapters/hardware/HardwareTestAdapter.h"

#include <algorithm>

namespace Siligen::Infrastructure::Adapters::Hardware {

HardwareTestAdapter::HardwareTestAdapter(
    std::shared_ptr<Siligen::Infrastructure::Hardware::IMultiCardWrapper> multicard,
    const Shared::Types::HardwareConfiguration& config,
    const std::array<Domain::Configuration::Ports::HomingConfig, 4>& homing_configs)
    : multicard_(multicard),
      hardware_config_(config),
      unit_converter_(config),  // 使用配置初始化单位转换器
      homing_configs_(homing_configs),  // 保存回零配置
      axis_count_(config.EffectiveAxisCount()),
      m_nextTriggerPointId(0),
      m_cmpTestRunning(false),
      m_cmpTestProgress(0.0) {}

}  // namespace Siligen::Infrastructure::Adapters::Hardware

