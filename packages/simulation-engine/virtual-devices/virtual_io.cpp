#include "sim/scheme_c/virtual_io.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace sim::scheme_c {
namespace {

struct IoRuntime {
    IoState state{};
    bool has_pending_output{false};
    bool pending_value{false};
};

class InMemoryVirtualIo final : public VirtualIo {
public:
    explicit InMemoryVirtualIo(const std::vector<std::string>& channels) {
        setChannels(channels);
    }

    void setChannels(const std::vector<std::string>& channels) override {
        channels_.clear();
        channel_indices_.clear();

        for (const auto& channel : channels) {
            if (channel_indices_.find(channel) != channel_indices_.end()) {
                continue;
            }

            IoRuntime runtime;
            runtime.state.channel = channel;
            runtime.state.output = true;
            channel_indices_.emplace(channel, channels_.size());
            channels_.push_back(runtime);
        }
    }

    void writeOutput(const std::string& channel, bool value) override {
        IoRuntime& runtime = ensureChannel(channel);
        runtime.state.output = true;
        if (!runtime.has_pending_output && runtime.state.value == value) {
            return;
        }
        if (runtime.has_pending_output && runtime.pending_value == value) {
            return;
        }

        runtime.pending_value = value;
        runtime.has_pending_output = true;
    }

    bool hasChannel(const std::string& channel) const override {
        return channel_indices_.find(channel) != channel_indices_.end();
    }

    IoState readChannel(const std::string& channel) const override {
        const IoRuntime* runtime = findChannel(channel);
        if (runtime == nullptr) {
            return IoState{channel, false, true};
        }

        return runtime->state;
    }

    void writeChannelState(const IoState& state) override {
        IoRuntime& runtime = ensureChannel(state.channel);
        runtime.state = state;
        runtime.has_pending_output = false;
    }

    bool readSignal(const std::string& channel) const override {
        const IoRuntime* runtime = findChannel(channel);
        return runtime != nullptr ? runtime->state.value : false;
    }

    void advance(const TickInfo& /*tick*/) override {
        for (auto& runtime : channels_) {
            if (!runtime.has_pending_output) {
                continue;
            }

            runtime.state.value = runtime.pending_value;
            runtime.has_pending_output = false;
        }
    }

    std::vector<IoState> snapshot() const override {
        std::vector<IoState> states;
        states.reserve(channels_.size());
        for (const auto& channel : channels_) {
            states.push_back(channel.state);
        }

        return states;
    }

private:
    const IoRuntime* findChannel(const std::string& channel) const {
        const auto it = channel_indices_.find(channel);
        if (it == channel_indices_.end()) {
            return nullptr;
        }

        return &channels_[it->second];
    }

    IoRuntime& ensureChannel(const std::string& channel) {
        const auto it = channel_indices_.find(channel);
        if (it != channel_indices_.end()) {
            return channels_[it->second];
        }

        IoRuntime runtime;
        runtime.state.channel = channel;
        runtime.state.output = true;
        channel_indices_.emplace(channel, channels_.size());
        channels_.push_back(runtime);
        return channels_.back();
    }

    std::vector<IoRuntime> channels_{};
    std::unordered_map<std::string, std::size_t> channel_indices_{};
};

}  // namespace

std::unique_ptr<VirtualIo> createInMemoryVirtualIo(
    const std::vector<std::string>& channels) {
    return std::make_unique<InMemoryVirtualIo>(channels);
}

}  // namespace sim::scheme_c
