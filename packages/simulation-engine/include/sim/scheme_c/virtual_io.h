#pragma once

#include <memory>
#include <string>
#include <vector>

#include "sim/scheme_c/types.h"

namespace sim::scheme_c {

class VirtualIo {
public:
    virtual ~VirtualIo() = default;

    virtual void setChannels(const std::vector<std::string>& channels) = 0;
    virtual void writeOutput(const std::string& channel, bool value) = 0;
    virtual bool hasChannel(const std::string& channel) const = 0;
    virtual IoState readChannel(const std::string& channel) const = 0;
    virtual void writeChannelState(const IoState& state) = 0;
    virtual bool readSignal(const std::string& channel) const = 0;
    virtual void advance(const TickInfo& tick) = 0;
    virtual std::vector<IoState> snapshot() const = 0;
};

std::unique_ptr<VirtualIo> createInMemoryVirtualIo(
    const std::vector<std::string>& channels = {});

}  // namespace sim::scheme_c
