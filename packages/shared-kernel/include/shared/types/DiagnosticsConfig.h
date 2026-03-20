#pragma once

#include "Types.h"

#include <string>

namespace Siligen::Shared::Types {

struct DiagnosticsConfig {
    bool deep_tcp_logging = false;
    bool deep_motion_logging = false;
    bool deep_hardware_logging = false;
    int32 snapshot_delay_ms = 120;
    int32 snapshot_after_stop_ms = 60;
    int32 tcp_payload_max_chars = 512;

    bool Validate() const {
        if (snapshot_delay_ms < 0 || snapshot_after_stop_ms < 0) {
            return false;
        }
        if (tcp_payload_max_chars <= 0) {
            return false;
        }
        return true;
    }

    std::string ToString() const {
        std::string result = "DiagnosticsConfig{";
        result += "deep_tcp=" + std::string(deep_tcp_logging ? "true" : "false");
        result += ", deep_motion=" + std::string(deep_motion_logging ? "true" : "false");
        result += ", deep_hardware=" + std::string(deep_hardware_logging ? "true" : "false");
        result += ", snapshot_delay_ms=" + std::to_string(snapshot_delay_ms);
        result += ", snapshot_after_stop_ms=" + std::to_string(snapshot_after_stop_ms);
        result += ", tcp_payload_max_chars=" + std::to_string(tcp_payload_max_chars);
        result += "}";
        return result;
    }
};

}  // namespace Siligen::Shared::Types
