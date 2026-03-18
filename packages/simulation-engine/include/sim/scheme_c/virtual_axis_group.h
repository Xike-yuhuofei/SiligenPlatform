#pragma once

#include <memory>
#include <string>
#include <vector>

#include "sim/scheme_c/types.h"

namespace sim::scheme_c {

class VirtualAxisGroup {
public:
    virtual ~VirtualAxisGroup() = default;

    virtual void setAxes(const std::vector<std::string>& axis_names) = 0;
    virtual void applyCommands(const std::vector<AxisCommand>& commands) = 0;
    virtual bool hasAxis(const std::string& axis_name) const = 0;
    virtual AxisState readAxis(const std::string& axis_name) const = 0;
    virtual void writeAxisState(const AxisState& state) = 0;
    virtual void advance(const TickInfo& tick) = 0;
    virtual std::vector<AxisState> snapshot() const = 0;
};

std::unique_ptr<VirtualAxisGroup> createInMemoryVirtualAxisGroup(
    const std::vector<std::string>& axis_names = {});

}  // namespace sim::scheme_c
