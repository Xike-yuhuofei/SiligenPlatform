#pragma once

#include "shared/types/Error.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <string>
#include <vector>

namespace Siligen::Domain::Dispensing::Ports {

using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::uint32;

struct ProfileCompareArmRequest {
    // 当前 span 的完整 future compare 程序。适配器负责一次或多次装载到硬件缓冲区。
    std::vector<long> compare_positions_pulse;
    // 正式 runtime compare 仅支持 X/Y compare 投影轴：1=X, 2=Y。
    short compare_source_axis = 0;
    uint32 expected_trigger_count = 0;
    uint32 start_boundary_trigger_count = 0;
    uint32 pulse_width_us = 2000;
    short start_level = 0;

    [[nodiscard]] bool IsValid() const noexcept {
        if (compare_source_axis < 1 || compare_source_axis > 2) {
            return false;
        }
        if (expected_trigger_count == 0U) {
            return false;
        }
        if (start_boundary_trigger_count > 1U) {
            return false;
        }
        const auto future_compare_count = static_cast<uint32>(compare_positions_pulse.size());
        if (expected_trigger_count != start_boundary_trigger_count + future_compare_count) {
            return false;
        }
        if (compare_positions_pulse.empty() &&
            start_boundary_trigger_count != expected_trigger_count) {
            return false;
        }
        if (pulse_width_us == 0U || pulse_width_us > 32767U) {
            return false;
        }
        if (start_level != 0 && start_level != 1) {
            return false;
        }

        long previous = 0;
        bool has_previous = false;
        for (const auto position_pulse : compare_positions_pulse) {
            if (position_pulse <= 0) {
                return false;
            }
            if (has_previous && position_pulse <= previous) {
                return false;
            }
            previous = position_pulse;
            has_previous = true;
        }
        return true;
    }

    [[nodiscard]] std::string GetValidationError() const {
        if (compare_source_axis < 1 || compare_source_axis > 2) {
            return "profile compare compare_source_axis must be 1 or 2 (X/Y)";
        }
        if (expected_trigger_count == 0U) {
            return "profile compare expected_trigger_count must be greater than 0";
        }
        if (start_boundary_trigger_count > 1U) {
            return "profile compare start_boundary_trigger_count must be 0 or 1";
        }
        const auto future_compare_count = static_cast<uint32>(compare_positions_pulse.size());
        if (expected_trigger_count != start_boundary_trigger_count + future_compare_count) {
            return "profile compare expected_trigger_count mismatch";
        }
        if (compare_positions_pulse.empty() &&
            start_boundary_trigger_count != expected_trigger_count) {
            return "profile compare positions can be empty only when start boundary covers all triggers";
        }
        if (pulse_width_us == 0U || pulse_width_us > 32767U) {
            return "profile compare pulse_width_us out of range";
        }
        if (start_level != 0 && start_level != 1) {
            return "profile compare start_level must be 0 or 1";
        }

        long previous = 0;
        bool has_previous = false;
        for (const auto position_pulse : compare_positions_pulse) {
            if (position_pulse <= 0) {
                return "profile compare positions must be greater than 0";
            }
            if (has_previous && position_pulse <= previous) {
                return "profile compare positions must be strictly increasing";
            }
            previous = position_pulse;
            has_previous = true;
        }
        return {};
    }
};

struct ProfileCompareStatus {
    bool armed = false;
    uint32 expected_trigger_count = 0;
    uint32 completed_trigger_count = 0;
    uint32 remaining_trigger_count = 0;
    uint32 submitted_future_compare_count = 0;
    unsigned short hardware_status_word = 0;
    unsigned short hardware_remain_data_1 = 0;
    unsigned short hardware_remain_data_2 = 0;
    unsigned short hardware_remain_space_1 = 0;
    unsigned short hardware_remain_space_2 = 0;
    std::string error_message;

    [[nodiscard]] bool Completed() const noexcept {
        return expected_trigger_count > 0U &&
               completed_trigger_count >= expected_trigger_count &&
               error_message.empty();
    }

    [[nodiscard]] bool HasError() const noexcept {
        return !error_message.empty();
    }
};

class IProfileComparePort {
   public:
    virtual ~IProfileComparePort() = default;

    virtual Result<void> ArmProfileCompare(const ProfileCompareArmRequest& request) noexcept = 0;
    virtual Result<void> DisarmProfileCompare() noexcept = 0;
    virtual Result<ProfileCompareStatus> GetProfileCompareStatus() noexcept = 0;
};

}  // namespace Siligen::Domain::Dispensing::Ports
