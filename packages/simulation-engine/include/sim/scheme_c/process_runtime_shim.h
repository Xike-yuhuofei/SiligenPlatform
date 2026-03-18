#pragma once

#include <memory>

#include "domain/motion/ports/IAxisControlPort.h"
#include "domain/motion/ports/IHomingPort.h"
#include "domain/motion/ports/IInterpolationPort.h"
#include "domain/motion/ports/IIOControlPort.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "sim/scheme_c/runtime_bridge.h"
#include "sim/scheme_c/virtual_axis_group.h"
#include "sim/scheme_c/virtual_io.h"

namespace sim::scheme_c {

struct ProcessRuntimeCoreShimBundle {
    RuntimeBridgeBindings resolved_bindings{};
    std::shared_ptr<Siligen::Domain::Motion::Ports::IHomingPort> homing{};
    std::shared_ptr<Siligen::Domain::Motion::Ports::IInterpolationPort> interpolation{};
    std::shared_ptr<Siligen::Domain::Motion::Ports::IAxisControlPort> axis_control{};
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionStatePort> motion_state{};
    std::shared_ptr<Siligen::Domain::Motion::Ports::IIOControlPort> io_control{};
};

ProcessRuntimeCoreShimBundle createProcessRuntimeCoreShimBundle(
    VirtualAxisGroup& axis_group,
    VirtualIo& io,
    const RuntimeBridgeBindings& bindings = {});

}  // namespace sim::scheme_c
