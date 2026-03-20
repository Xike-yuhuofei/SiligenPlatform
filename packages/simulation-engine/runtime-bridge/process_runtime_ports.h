#pragma once

#include <memory>

#include "domain/motion/ports/IInterpolationPort.h"
#include "domain/motion/ports/IMotionRuntimePort.h"
#include "sim/scheme_c/runtime_bridge.h"

namespace sim::scheme_c {

class VirtualAxisGroup;
class VirtualIo;

struct ProcessRuntimeCorePortAdapters {
    RuntimeBridgeBindings resolved_bindings{};
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionRuntimePort> motion_runtime{};
    std::shared_ptr<Siligen::Domain::Motion::Ports::IInterpolationPort> interpolation{};
};

ProcessRuntimeCorePortAdapters createProcessRuntimeCorePortAdapters(
    VirtualAxisGroup& axis_group,
    VirtualIo& io,
    const RuntimeBridgeBindings& bindings = {});

}  // namespace sim::scheme_c
