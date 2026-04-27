// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#define MODULE_NAME "ValveAdapter"

#include "ValveAdapter.h"
#include "shared/interfaces/ILoggingService.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <chrono>
#include <thread>

namespace Siligen {
namespace Infrastructure {
namespace Adapters {

using namespace Domain::Dispensing::Ports;
using namespace Shared::Types;

// ============================================================
// 私有辅助方法
// ============================================================

namespace {

constexpr short kSdkCmpPulseType = 2;
constexpr short kSdkCmpTimerFlagMicroseconds = 0;
constexpr auto kProfileCompareWorkerPollInterval = std::chrono::milliseconds(2);

struct CompareBufferChunk {
    std::vector<long> buffer1;
    std::vector<long> buffer2;
};

CompareBufferChunk TakeCompareChunk(const std::vector<long>& compare_positions,
                                    std::size_t submitted_count,
                                    unsigned short space1,
                                    unsigned short space2) {
    CompareBufferChunk chunk;
    std::size_t cursor = submitted_count;
    const auto total_count = compare_positions.size();

    const auto buffer1_count = std::min<std::size_t>(
        static_cast<std::size_t>(space1),
        total_count - cursor);
    chunk.buffer1.insert(
        chunk.buffer1.end(),
        compare_positions.begin() + static_cast<std::ptrdiff_t>(cursor),
        compare_positions.begin() + static_cast<std::ptrdiff_t>(cursor + buffer1_count));
    cursor += buffer1_count;

    const auto buffer2_count = std::min<std::size_t>(
        static_cast<std::size_t>(space2),
        total_count - cursor);
    chunk.buffer2.insert(
        chunk.buffer2.end(),
        compare_positions.begin() + static_cast<std::ptrdiff_t>(cursor),
        compare_positions.begin() + static_cast<std::ptrdiff_t>(cursor + buffer2_count));
    return chunk;
}

int32 CompareAxisMaskBit(short compare_source_axis) noexcept {
    switch (compare_source_axis) {
        case 1:
            return 0x01;
        case 2:
            return 0x02;
        case 3:
            return 0x04;
        case 4:
            return 0x08;
        default:
            return 0;
    }
}

std::string JoinCompareContextFields(const std::initializer_list<std::string>& fields) {
    std::string context;
    bool first = true;
    for (const auto& field : fields) {
        if (field.empty()) {
            continue;
        }
        if (!first) {
            context += ", ";
        }
        context += field;
        first = false;
    }
    return context;
}

int CallCmpPulseOnce(Siligen::Infrastructure::Hardware::IMultiCardWrapper& hardware,
                     const DispenserValveConfig& dispenser_config,
                     int16 channel,
                     uint32 duration,
                     short start_level,
                     short time_flag,
                     const char* duration_unit) {
    try {
        if (duration == 0U || duration > static_cast<uint32>(std::numeric_limits<short>::max())) {
            SILIGEN_LOG_WARNING(std::string("CallCmpPulseOnce: 脉冲宽度超出 SDK short 范围, duration=") +
                                std::to_string(duration));
            return 7;
        }

        const short selected_channel = (channel > 0)
                                           ? channel
                                           : static_cast<short>(dispenser_config.cmp_channel);
        const short normalized_start_level =
            (start_level == 0 || start_level == 1)
                ? start_level
                : static_cast<short>((dispenser_config.pulse_type == 0) ? 0 : 1);
        short nChannelMask = 0;
        short presetType1 = 0;
        short presetType2 = 0;
        short nPluseType1 = 0;
        short nPluseType2 = 0;
        short nTime1 = 0;
        short nTime2 = 0;
        short nTimeFlag1 = 0;
        short nTimeFlag2 = 0;

        if (selected_channel == 1) {
            nChannelMask = 0x01;
            presetType1 = normalized_start_level;
            nPluseType1 = 2;
            nTime1 = static_cast<short>(duration);
            nTimeFlag1 = time_flag;
        } else if (selected_channel == 2) {
            nChannelMask = 0x02;
            presetType2 = normalized_start_level;
            nPluseType2 = 2;
            nTime2 = static_cast<short>(duration);
            nTimeFlag2 = time_flag;
        } else {
            SILIGEN_LOG_WARNING("CallCmpPulseOnce: 不支持的CMP通道=" + std::to_string(selected_channel) +
                                " (仅支持1/2)");
            return 7;
        }

        const int preset_result = hardware.MC_CmpPluse(
            nChannelMask, presetType1, presetType2, 0, 0, 0, 0);
        if (preset_result != 0) {
            SILIGEN_LOG_ERROR("CallCmpPulseOnce: 预设起始电平失败: " + std::to_string(preset_result));
            return preset_result;
        }

        SILIGEN_LOG_INFO(std::string("CallCmpPulseOnce: 调用 MC_CmpPluse 单次脉冲: Channel=") +
                         std::to_string(selected_channel) +
                         ", PulseWidth=" + std::to_string(duration) + duration_unit +
                         ", StartLevel=" + std::to_string(normalized_start_level));

        const int result = hardware.MC_CmpPluse(nChannelMask, nPluseType1, nPluseType2,
                                                nTime1, nTime2, nTimeFlag1, nTimeFlag2);
        if (result != 0) {
            SILIGEN_LOG_ERROR("CallCmpPulseOnce: MC_CmpPluse 失败: " + std::to_string(result));
        }
        return result;
    }
    catch (...) {
        return -9999;
    }
}

}  // namespace

int ValveAdapter::CallMC_CmpPulseOnce(int16 channel, uint32 durationMs) {
    const short default_start_level =
        static_cast<short>((dispenser_config_.pulse_type == 0) ? 0 : 1);
    return CallMC_CmpPulseOnce(channel, durationMs, default_start_level);
}

int ValveAdapter::CallMC_CmpPulseOnce(int16 channel, uint32 durationMs, short startLevel) {
    return CallCmpPulseOnce(*hardware_, dispenser_config_, channel, durationMs, startLevel, 1, "ms");
}

int ValveAdapter::CallMC_CmpPulseOnceMicroseconds(int16 channel, uint32 durationUs, short startLevel) {
    return CallCmpPulseOnce(*hardware_, dispenser_config_, channel, durationUs, startLevel, 0, "us");
}

int ValveAdapter::CallMC_SetCmpLevel(bool high) {
    try {
        bool active_high = (dispenser_config_.pulse_type == 0);
        short level = high ? (active_high ? 1 : 0) : (active_high ? 0 : 1);
        short nChannelMask = 0;
        short nPluseType1 = 0;
        short nPluseType2 = 0;
        short nTime1 = 0;
        short nTime2 = 0;
        short nTimeFlag1 = 0;
        short nTimeFlag2 = 0;

        const short selected_channel = static_cast<short>(dispenser_config_.cmp_channel);
        if (selected_channel == 1) {
            nChannelMask = 0x01;
            nPluseType1 = level;
        } else if (selected_channel == 2) {
            nChannelMask = 0x02;
            nPluseType2 = level;
        } else {
            SILIGEN_LOG_WARNING("CallMC_SetCmpLevel: 不支持的CMP通道=" + std::to_string(selected_channel) +
                                " (仅支持1/2)");
            return 7;  // 参数错误
        }

        SILIGEN_LOG_INFO(std::string("CallMC_SetCmpLevel: channel=") +
                         std::to_string(selected_channel) +
                         ", level=" + (level == 1 ? "HIGH" : "LOW") +
                         ", active_high=" + std::to_string(active_high));

        return hardware_->MC_CmpPluse(nChannelMask, nPluseType1, nPluseType2,
                                      nTime1, nTime2, nTimeFlag1, nTimeFlag2);
    }
    catch (...) {
        return -9999;
    }
}

int ValveAdapter::CallMC_SetExtDoBit(int16 value) {
    try {
        // 使用 MC_SetExtDoBit API
        // MC_SetExtDoBit(short nCardIndex, short nBitIndex, unsigned short nValue)
        //
        // 参数：
        // - nCardIndex: 卡索引（0=主卡，1=扩展卡1，...）
        // - nBitIndex: DO位索引（0-based，从配置获取）
        // - nValue: 输出值（0或1）

        return hardware_->MC_SetExtDoBit(
            static_cast<short>(supply_config_.do_card_index),  // 从配置获取
            static_cast<short>(supply_config_.do_bit_index),   // 从配置获取
            static_cast<unsigned short>(value)
        );
    }
    catch (...) {
        return -9999;
    }
}

unsigned short ValveAdapter::BuildCmpChannelMask() const noexcept {
    unsigned short mask = 0x01;  // 通道1（bit0）
    int32 cmp_channel = dispenser_config_.cmp_channel;
    if (cmp_channel >= 1 && cmp_channel <= 16) {
        mask |= static_cast<unsigned short>(1U << static_cast<unsigned short>(cmp_channel - 1));
    }
    return mask;
}

int ValveAdapter::CallMC_StopCmpOutputs(unsigned short channelMask) {
    try {
        // 使用 MC_CmpBufStop API 停止 CMP 触发
        // MC_CmpBufStop(short nChannelMask)
        return hardware_->MC_CmpBufStop(channelMask);
    }
    catch (...) {
        return -9999;
    }
}

short ValveAdapter::ResolvePrimaryCompareSourceAxis(const ProfileCompareArmRequest& request) const noexcept {
    if (request.compare_source_axis < 1 || request.compare_source_axis > 4) {
        return 0;
    }

    const int32 mask_bit = CompareAxisMaskBit(request.compare_source_axis);
    if (mask_bit == 0) {
        return 0;
    }

    return (dispenser_config_.cmp_axis_mask & mask_bit) != 0
        ? request.compare_source_axis
        : 0;
}

void ValveAdapter::ResetProfileCompareTrackingState() noexcept {
    profile_compare_status_ = {};
    profile_compare_start_boundary_trigger_count_ = 0U;
    profile_compare_worker_active_ = false;
    profile_compare_session_ = {};
}

void ValveAdapter::UpdateProfileCompareStatusFromHardwareSnapshot(unsigned short status,
                                                                  unsigned short remain_data_1,
                                                                  unsigned short remain_data_2,
                                                                  unsigned short remain_space_1,
                                                                  unsigned short remain_space_2) noexcept {
    profile_compare_session_.last_status = status;
    profile_compare_session_.last_remain_data_1 = remain_data_1;
    profile_compare_session_.last_remain_data_2 = remain_data_2;
    profile_compare_session_.last_remain_space_1 = remain_space_1;
    profile_compare_session_.last_remain_space_2 = remain_space_2;

    const auto submitted_future_compare_count =
        static_cast<uint32>(profile_compare_session_.submitted_future_compare_count);
    const auto queued_future_compare_count =
        static_cast<uint32>(remain_data_1) + static_cast<uint32>(remain_data_2);
    const auto completed_future_compare_count =
        submitted_future_compare_count > queued_future_compare_count
            ? submitted_future_compare_count - queued_future_compare_count
            : 0U;

    profile_compare_status_.submitted_future_compare_count = submitted_future_compare_count;
    profile_compare_status_.hardware_status_word = status;
    profile_compare_status_.hardware_remain_data_1 = remain_data_1;
    profile_compare_status_.hardware_remain_data_2 = remain_data_2;
    profile_compare_status_.hardware_remain_space_1 = remain_space_1;
    profile_compare_status_.hardware_remain_space_2 = remain_space_2;
    profile_compare_status_.completed_trigger_count =
        profile_compare_start_boundary_trigger_count_ + completed_future_compare_count;
    if (profile_compare_status_.completed_trigger_count > profile_compare_status_.expected_trigger_count) {
        profile_compare_status_.completed_trigger_count = profile_compare_status_.expected_trigger_count;
    }
    profile_compare_status_.remaining_trigger_count =
        profile_compare_status_.expected_trigger_count - profile_compare_status_.completed_trigger_count;
    profile_compare_status_.armed =
        profile_compare_session_.active &&
        (profile_compare_status_.remaining_trigger_count > 0U);
}

void ValveAdapter::StopProfileCompareWorker() noexcept {
    std::thread worker_to_join;
    {
        std::lock_guard<std::mutex> lock(dispenser_mutex_);
        if (profile_compare_worker_thread_.joinable()) {
            profile_compare_session_.stop_requested = true;
            worker_to_join = std::move(profile_compare_worker_thread_);
        } else {
            profile_compare_session_.stop_requested = true;
        }
    }

    if (worker_to_join.joinable()) {
        worker_to_join.join();
    }

    std::lock_guard<std::mutex> lock(dispenser_mutex_);
    profile_compare_worker_active_ = false;
    profile_compare_session_.stop_requested = false;
}

void ValveAdapter::ProfileCompareWorkerLoop() noexcept {
    while (true) {
        short compare_source_axis = 0;
        short sdk_pulse_type = 0;
        short start_level = 0;
        short pulse_width_us = 0;
        short abs_position_flag = 0;
        short timer_flag = 0;
        std::size_t submitted_count = 0U;
        std::vector<long> compare_positions;
        std::string compare_context;

        {
            std::lock_guard<std::mutex> lock(dispenser_mutex_);
            if (profile_compare_session_.stop_requested || !profile_compare_session_.active) {
                profile_compare_worker_active_ = false;
                return;
            }
            compare_source_axis = profile_compare_session_.compare_source_axis;
            sdk_pulse_type = profile_compare_session_.sdk_pulse_type;
            start_level = profile_compare_session_.start_level;
            pulse_width_us = profile_compare_session_.pulse_width_us;
            abs_position_flag = profile_compare_session_.abs_position_flag;
            timer_flag = profile_compare_session_.timer_flag;
            submitted_count = profile_compare_session_.submitted_future_compare_count;
            compare_positions = profile_compare_session_.compare_positions_pulse;
            compare_context = profile_compare_session_.compare_context;
        }

        unsigned short status = 0;
        unsigned short remain_data_1 = 0;
        unsigned short remain_data_2 = 0;
        unsigned short remain_space_1 = 0;
        unsigned short remain_space_2 = 0;
        const int status_result = hardware_->MC_CmpBufSts(
            &status,
            &remain_data_1,
            &remain_data_2,
            &remain_space_1,
            &remain_space_2);
        if (status_result != 0) {
            std::lock_guard<std::mutex> lock(dispenser_mutex_);
            profile_compare_status_.error_message =
                CreateErrorMessage(status_result, "MC_CmpBufSts");
            profile_compare_status_.armed = false;
            profile_compare_worker_active_ = false;
            profile_compare_session_.active = false;
            return;
        }

        bool should_exit = false;
        {
            std::lock_guard<std::mutex> lock(dispenser_mutex_);
            UpdateProfileCompareStatusFromHardwareSnapshot(
                status,
                remain_data_1,
                remain_data_2,
                remain_space_1,
                remain_space_2);

            if (profile_compare_session_.stop_requested || !profile_compare_session_.active) {
                profile_compare_worker_active_ = false;
                return;
            }

            if (profile_compare_session_.submitted_future_compare_count >=
                    profile_compare_session_.compare_positions_pulse.size() &&
                remain_data_1 == 0U && remain_data_2 == 0U) {
                profile_compare_status_.armed = false;
                profile_compare_worker_active_ = false;
                profile_compare_session_.active = false;
                return;
            }

            should_exit = profile_compare_session_.submitted_future_compare_count >=
                              profile_compare_session_.compare_positions_pulse.size() ||
                          (remain_space_1 == 0U && remain_space_2 == 0U);
        }

        if (!should_exit) {
            auto chunk = TakeCompareChunk(
                compare_positions,
                submitted_count,
                remain_space_1,
                remain_space_2);
            if (!chunk.buffer1.empty() || !chunk.buffer2.empty()) {
                const int refill_result = hardware_->MC_CmpBufData(
                    compare_source_axis,
                    sdk_pulse_type,
                    start_level,
                    pulse_width_us,
                    chunk.buffer1.empty() ? nullptr : chunk.buffer1.data(),
                    static_cast<short>(chunk.buffer1.size()),
                    chunk.buffer2.empty() ? nullptr : chunk.buffer2.data(),
                    static_cast<short>(chunk.buffer2.size()),
                    abs_position_flag,
                    timer_flag);
                if (refill_result != 0) {
                    std::lock_guard<std::mutex> lock(dispenser_mutex_);
                    profile_compare_status_.error_message =
                        CreateErrorMessage(refill_result, "MC_CmpBufData") + "; " + compare_context;
                    profile_compare_status_.armed = false;
                    profile_compare_worker_active_ = false;
                    profile_compare_session_.active = false;
                    return;
                }

                std::lock_guard<std::mutex> lock(dispenser_mutex_);
                profile_compare_session_.submitted_future_compare_count +=
                    chunk.buffer1.size() + chunk.buffer2.size();
                UpdateProfileCompareStatusFromHardwareSnapshot(
                    status,
                    static_cast<unsigned short>(remain_data_1 + chunk.buffer1.size()),
                    static_cast<unsigned short>(remain_data_2 + chunk.buffer2.size()),
                    remain_space_1 >= chunk.buffer1.size()
                        ? static_cast<unsigned short>(remain_space_1 - chunk.buffer1.size())
                        : 0,
                    remain_space_2 >= chunk.buffer2.size()
                        ? static_cast<unsigned short>(remain_space_2 - chunk.buffer2.size())
                        : 0);
            }
        }

        std::this_thread::sleep_for(kProfileCompareWorkerPollInterval);
    }
}

bool ValveAdapter::UseAbsoluteProfileComparePositioning() const noexcept {
    return dispenser_config_.abs_position_flag != 0;
}

std::string ValveAdapter::BuildProfileCompareContext(const ProfileCompareArmRequest& request,
                                                     short compare_source_axis,
                                                     short cmp_channel,
                                                     long base_profile_position_pulse,
                                                     std::size_t future_compare_count,
                                                     short abs_position_flag,
                                                     short timer_flag,
                                             short sdk_pulse_type,
                                             const std::vector<long>* compare_positions) {
    long first_compare_position = 0;
    long last_compare_position = 0;
    if (compare_positions && !compare_positions->empty()) {
        first_compare_position = compare_positions->front();
        last_compare_position = compare_positions->back();
    }

    return JoinCompareContextFields({
        "source_axis=" + std::to_string(compare_source_axis),
        "cmp_channel=" + std::to_string(cmp_channel),
        "business_trigger_count=" + std::to_string(request.expected_trigger_count),
        "start_boundary_trigger_count=" + std::to_string(request.start_boundary_trigger_count),
        "future_compare_count=" + std::to_string(future_compare_count),
        "pulse_width_us=" + std::to_string(request.pulse_width_us),
        "start_level=" + std::to_string(request.start_level),
        "abs_position_flag=" + std::to_string(abs_position_flag),
        "timer_flag=" + std::to_string(timer_flag),
        "sdk_pulse_type=" + std::to_string(sdk_pulse_type),
        "base_profile_position_pulse=" + std::to_string(base_profile_position_pulse),
        "first_compare_position=" + std::to_string(first_compare_position),
        "last_compare_position=" + std::to_string(last_compare_position),
    });
}

Result<void> ValveAdapter::ValidateProfileCompareRequestForHardware(
    const ProfileCompareArmRequest& request,
    short compare_source_axis,
    short cmp_channel,
    short abs_position_flag,
    short timer_flag,
    short sdk_pulse_type,
    long base_profile_position_pulse,
    const std::vector<long>& compare_positions) const noexcept {
    if (compare_source_axis <= 0) {
        return Result<void>::Failure(
            Shared::Types::Error(ErrorCode::INVALID_PARAMETER,
                                 "profile compare request.compare_source_axis 不在 ValveDispenser.cmp_axis_mask 允许范围内",
                                 "ValveAdapter"));
    }

    if (cmp_channel < 1 || cmp_channel > 2) {
        return Result<void>::Failure(
            Shared::Types::Error(ErrorCode::INVALID_CONFIG_VALUE,
                                 "ValveDispenser.cmp_channel 仅支持 1 或 2",
                                 "ValveAdapter"));
    }

    if (sdk_pulse_type != kSdkCmpPulseType) {
        return Result<void>::Failure(
            Shared::Types::Error(ErrorCode::INVALID_PARAMETER,
                                 "profile compare 仅支持 MultiCard 脉冲输出模式",
                                 "ValveAdapter"));
    }

    if (timer_flag != kSdkCmpTimerFlagMicroseconds) {
        return Result<void>::Failure(
            Shared::Types::Error(ErrorCode::INVALID_PARAMETER,
                                 "profile compare 仅支持微秒时间单位",
                                 "ValveAdapter"));
    }

    if (abs_position_flag != 0 && abs_position_flag != 1) {
        return Result<void>::Failure(
            Shared::Types::Error(ErrorCode::INVALID_CONFIG_VALUE,
                                 "ValveDispenser.abs_position_flag 必须为 0 或 1",
                                 "ValveAdapter"));
    }

    if (request.expected_trigger_count < request.start_boundary_trigger_count) {
        return Result<void>::Failure(
            Shared::Types::Error(ErrorCode::INVALID_PARAMETER,
                                 "profile compare expected_trigger_count 不能小于起点边界触发数",
                                 "ValveAdapter"));
    }

    const auto future_compare_count = static_cast<uint32>(compare_positions.size());
    if (future_compare_count == 0U) {
        return Result<void>::Success();
    }

    if (dispenser_config_.max_count < 1) {
        return Result<void>::Failure(
            Shared::Types::Error(ErrorCode::INVALID_CONFIG_VALUE,
                                 "ValveDispenser.max_count 必须大于 0",
                                 "ValveAdapter"));
    }

    if (future_compare_count > static_cast<uint32>(std::numeric_limits<short>::max())) {
        return Result<void>::Failure(
            Shared::Types::Error(ErrorCode::INVALID_PARAMETER,
                                 "profile compare future_compare_count 超出 SDK short 范围",
                                 "ValveAdapter"));
    }

    if (future_compare_count > static_cast<uint32>(dispenser_config_.max_count)) {
        return Result<void>::Failure(
            Shared::Types::Error(
                ErrorCode::INVALID_PARAMETER,
                "profile compare future_compare_count 超出 ValveDispenser.max_count: future_compare_count=" +
                    std::to_string(future_compare_count) +
                    ", max_count=" + std::to_string(dispenser_config_.max_count),
                "ValveAdapter"));
    }

    long previous_compare_position = 0;
    bool has_previous_compare_position = false;
    for (const auto compare_position : compare_positions) {
        if (abs_position_flag == 1 && compare_position <= base_profile_position_pulse) {
            return Result<void>::Failure(
                Shared::Types::Error(
                    ErrorCode::INVALID_PARAMETER,
                    "profile compare absolute position 必须严格晚于当前规划位置: compare_position=" +
                        std::to_string(compare_position) +
                        ", base_profile_position_pulse=" + std::to_string(base_profile_position_pulse),
                    "ValveAdapter"));
        }

        if (has_previous_compare_position && compare_position <= previous_compare_position) {
            return Result<void>::Failure(
                Shared::Types::Error(ErrorCode::INVALID_PARAMETER,
                                     "profile compare 下发到硬件的 compare 位置必须严格递增",
                                     "ValveAdapter"));
        }

        previous_compare_position = compare_position;
        has_previous_compare_position = true;
    }

    return Result<void>::Success();
}

int ValveAdapter::ResetDispenserHardwareState(const char* reason, bool strict) noexcept {
    const unsigned short channel_mask = BuildCmpChannelMask();

    // 1. 停止 CMP 位置比较触发
    int stop_result = CallMC_StopCmpOutputs(channel_mask);
    if (stop_result != 0) {
        SILIGEN_LOG_WARNING(std::string(reason) + ": MC_CmpBufStop 返回 " + std::to_string(stop_result));
        if (strict) {
            return stop_result;
        }
    }

    // 2. 断开 CMP 缓冲区输出通道（解决 MC_CmpBufData 电平残留问题）
    // MC_CmpBufSetChannel(0, 0) 将两个缓冲区的输出通道都设为0（无输出）
    // 这确保了即使 MC_CmpBufData 产生的电平状态残留，也不会输出到物理端口
    int channel_result = hardware_->MC_CmpBufSetChannel(0, 0);
    if (channel_result != 0) {
        SILIGEN_LOG_WARNING(std::string(reason) + ": MC_CmpBufSetChannel(0,0) 返回 " + std::to_string(channel_result));
        // 不严格要求成功，继续执行后续步骤
    }

    // 3. 拉低 CMP 脉冲输出电平（备用保险）
    int level_result = CallMC_SetCmpLevel(false);
    if (level_result != 0) {
        SILIGEN_LOG_WARNING(std::string(reason) + ": MC_CmpPluse(关阀) 返回 " + std::to_string(level_result));
        if (strict) {
            return level_result;
        }
    }

    if (compensation_profile_.retract_enabled && compensation_profile_.close_comp_ms > 0.0f) {
        auto retract_ms = static_cast<int>(std::llround(compensation_profile_.close_comp_ms));
        if (retract_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(retract_ms));
        }
    }

    return 0;
}

Result<void> ValveAdapter::ArmProfileCompare(const ProfileCompareArmRequest& request) noexcept {
    try {
        StopProfileCompareWorker();
        JoinTimedDispenserThreadIfFinished();
        JoinPathTriggeredDispenserThreadIfFinished();

        std::lock_guard<std::mutex> lock(dispenser_mutex_);

        auto fail_profile_compare = [&](ErrorCode code, const std::string& error_message) -> Result<void> {
            ResetProfileCompareTrackingState();
            profile_compare_status_.error_message = error_message;
            return Result<void>::Failure(
                Shared::Types::Error(code, error_message, "ValveAdapter"));
        };

        if (!request.IsValid()) {
            const auto error_message = request.GetValidationError();
            return fail_profile_compare(ErrorCode::INVALID_PARAMETER, error_message);
        }

        if (dispenser_state_.status == DispenserValveStatus::Running ||
            dispenser_state_.status == DispenserValveStatus::Paused) {
            return fail_profile_compare(ErrorCode::INVALID_STATE, "点胶阀已在运行中");
        }

        ResetProfileCompareTrackingState();

        const short compare_source_axis = ResolvePrimaryCompareSourceAxis(request);

        const int reset_result = ResetDispenserHardwareState("ArmProfileCompare", false);
        if (reset_result != 0) {
            SILIGEN_LOG_WARNING("ArmProfileCompare: ResetDispenserHardwareState 返回非零值 " +
                                std::to_string(reset_result) + ",继续尝试装载 compare 程序");
        }

        const short cmp_channel = static_cast<short>(dispenser_config_.cmp_channel);
        const short abs_position_flag = UseAbsoluteProfileComparePositioning() ? 1 : 0;
        const short timer_flag = kSdkCmpTimerFlagMicroseconds;
        const short sdk_pulse_type = kSdkCmpPulseType;
        const auto static_validation_result = ValidateProfileCompareRequestForHardware(
            request,
            compare_source_axis,
            cmp_channel,
            abs_position_flag,
            timer_flag,
            sdk_pulse_type,
            0,
            request.compare_positions_pulse);
        if (static_validation_result.IsError()) {
            return fail_profile_compare(
                static_validation_result.GetError().GetCode(),
                static_validation_result.GetError().GetMessage());
        }

        if (request.start_boundary_trigger_count == 1U) {
            const int pulse_result = CallMC_CmpPulseOnceMicroseconds(
                cmp_channel,
                request.pulse_width_us,
                request.start_level);
            if (pulse_result != 0) {
                const auto error_message = CreateErrorMessage(pulse_result, "MC_CmpPluse");
                return fail_profile_compare(ErrorCode::HARDWARE_ERROR, error_message);
            }
        }

        if (request.compare_positions_pulse.empty()) {
            profile_compare_start_boundary_trigger_count_ = request.start_boundary_trigger_count;
            profile_compare_status_.armed = false;
            profile_compare_status_.expected_trigger_count = request.expected_trigger_count;
            profile_compare_status_.completed_trigger_count = request.expected_trigger_count;
            profile_compare_status_.remaining_trigger_count = 0U;
            profile_compare_status_.error_message.clear();

            SILIGEN_LOG_INFO("ArmProfileCompare: immediate-only request, source_axis=" +
                             std::to_string(compare_source_axis) +
                             ", cmp_channel=" + std::to_string(cmp_channel) +
                             ", business_trigger_count=" + std::to_string(request.expected_trigger_count) +
                             ", start_boundary_trigger_count=" +
                             std::to_string(request.start_boundary_trigger_count) +
                             ", future_compare_count=0" +
                             ", pulse_width_us=" + std::to_string(request.pulse_width_us));
            return Result<void>::Success();
        }

        const int encoder_result = hardware_->MC_EncOff(compare_source_axis);
        if (encoder_result != 0) {
            const auto error_message = CreateErrorMessage(encoder_result, "MC_EncOff");
            return fail_profile_compare(ErrorCode::HARDWARE_ERROR, error_message);
        }

        const int channel_result = hardware_->MC_CmpBufSetChannel(cmp_channel, cmp_channel);
        if (channel_result != 0) {
            const auto error_message = CreateErrorMessage(channel_result, "MC_CmpBufSetChannel");
            return fail_profile_compare(ErrorCode::HARDWARE_ERROR, error_message);
        }

        std::vector<long> compare_positions = request.compare_positions_pulse;
        long current_profile_position_pulse = 0;
        if (abs_position_flag == 1) {
            double current_profile_position = 0.0;
            const int profile_position_result = hardware_->MC_GetPrfPos(compare_source_axis, &current_profile_position);
            if (profile_position_result != 0) {
                const auto error_message = CreateErrorMessage(profile_position_result, "MC_GetPrfPos");
                return fail_profile_compare(ErrorCode::HARDWARE_ERROR, error_message);
            }

            const auto rounded_current_profile_position = std::llround(current_profile_position);
            if (rounded_current_profile_position < static_cast<long long>(std::numeric_limits<long>::min()) ||
                rounded_current_profile_position > static_cast<long long>(std::numeric_limits<long>::max())) {
                return fail_profile_compare(
                    ErrorCode::INVALID_PARAMETER,
                    "MC_GetPrfPos 返回值超出 long 范围");
            }

            current_profile_position_pulse = static_cast<long>(rounded_current_profile_position);

            // BuildProfileCompareArmRequest 产出的是从当前工艺起点累计的 compare 距离；
            // 真实板卡在 abs_position_flag=1 下要求绝对 compare 位置，因此需叠加当前规划位置。
            for (auto& compare_position : compare_positions) {
                const auto absolute_compare_position =
                    static_cast<long long>(current_profile_position_pulse) + static_cast<long long>(compare_position);
                if (absolute_compare_position < static_cast<long long>(std::numeric_limits<long>::min()) ||
                    absolute_compare_position > static_cast<long long>(std::numeric_limits<long>::max())) {
                    return fail_profile_compare(
                        ErrorCode::INVALID_PARAMETER,
                        "profile compare absolute position 超出 long 范围");
                }
                compare_position = static_cast<long>(absolute_compare_position);
            }
        }

        const auto validation_result = ValidateProfileCompareRequestForHardware(
            request,
            compare_source_axis,
            cmp_channel,
            abs_position_flag,
            timer_flag,
            sdk_pulse_type,
            current_profile_position_pulse,
            compare_positions);
        if (validation_result.IsError()) {
            const auto context = BuildProfileCompareContext(
                request,
                compare_source_axis,
                cmp_channel,
                current_profile_position_pulse,
                compare_positions.size(),
                abs_position_flag,
                timer_flag,
                sdk_pulse_type,
                &compare_positions);
            return fail_profile_compare(
                validation_result.GetError().GetCode(),
                validation_result.GetError().GetMessage() + "; " + context);
        }

        const auto compare_context = BuildProfileCompareContext(
            request,
            compare_source_axis,
            cmp_channel,
            current_profile_position_pulse,
            compare_positions.size(),
            abs_position_flag,
            timer_flag,
            sdk_pulse_type,
            &compare_positions);

        unsigned short status = 0;
        unsigned short remain_data_1 = 0;
        unsigned short remain_data_2 = 0;
        unsigned short remain_space_1 = 0;
        unsigned short remain_space_2 = 0;
        const int buffer_status_result = hardware_->MC_CmpBufSts(
            &status,
            &remain_data_1,
            &remain_data_2,
            &remain_space_1,
            &remain_space_2);
        if (buffer_status_result != 0) {
            const auto error_message = CreateErrorMessage(buffer_status_result, "MC_CmpBufSts");
            return fail_profile_compare(ErrorCode::HARDWARE_ERROR, error_message + "; " + compare_context);
        }

        auto initial_chunk = TakeCompareChunk(compare_positions, 0U, remain_space_1, remain_space_2);
        if (initial_chunk.buffer1.empty() && initial_chunk.buffer2.empty()) {
            return fail_profile_compare(
                ErrorCode::CMP_TRIGGER_SETUP_FAILED,
                "profile compare 板卡 compare buffer 无可用空间; " + compare_context);
        }

        const int arm_result = hardware_->MC_CmpBufData(
            compare_source_axis,
            sdk_pulse_type,
            request.start_level,
            static_cast<short>(request.pulse_width_us),
            initial_chunk.buffer1.empty() ? nullptr : initial_chunk.buffer1.data(),
            static_cast<short>(initial_chunk.buffer1.size()),
            initial_chunk.buffer2.empty() ? nullptr : initial_chunk.buffer2.data(),
            static_cast<short>(initial_chunk.buffer2.size()),
            abs_position_flag,
            timer_flag);
        if (arm_result != 0) {
            const auto error_message = CreateErrorMessage(arm_result, "MC_CmpBufData");
            return fail_profile_compare(ErrorCode::HARDWARE_ERROR, error_message + "; " + compare_context);
        }

        profile_compare_start_boundary_trigger_count_ = request.start_boundary_trigger_count;
        profile_compare_status_.armed = true;
        profile_compare_status_.expected_trigger_count = request.expected_trigger_count;
        profile_compare_status_.completed_trigger_count = request.start_boundary_trigger_count;
        profile_compare_status_.remaining_trigger_count =
            request.expected_trigger_count - request.start_boundary_trigger_count;
        profile_compare_status_.error_message.clear();

        profile_compare_session_.active = true;
        profile_compare_session_.stop_requested = false;
        profile_compare_session_.compare_source_axis = compare_source_axis;
        profile_compare_session_.cmp_channel = cmp_channel;
        profile_compare_session_.abs_position_flag = abs_position_flag;
        profile_compare_session_.timer_flag = timer_flag;
        profile_compare_session_.sdk_pulse_type = sdk_pulse_type;
        profile_compare_session_.start_level = request.start_level;
        profile_compare_session_.pulse_width_us = static_cast<short>(request.pulse_width_us);
        profile_compare_session_.compare_positions_pulse = compare_positions;
        profile_compare_session_.submitted_future_compare_count =
            initial_chunk.buffer1.size() + initial_chunk.buffer2.size();
        profile_compare_session_.compare_context = compare_context;
        UpdateProfileCompareStatusFromHardwareSnapshot(
            status,
            remain_data_1 + static_cast<unsigned short>(initial_chunk.buffer1.size()),
            remain_data_2 + static_cast<unsigned short>(initial_chunk.buffer2.size()),
            remain_space_1 >= initial_chunk.buffer1.size()
                ? static_cast<unsigned short>(remain_space_1 - initial_chunk.buffer1.size())
                : 0,
            remain_space_2 >= initial_chunk.buffer2.size()
                ? static_cast<unsigned short>(remain_space_2 - initial_chunk.buffer2.size())
                : 0);

        profile_compare_worker_active_ = true;
        try {
            profile_compare_worker_thread_ = std::thread([this]() { ProfileCompareWorkerLoop(); });
        }
        catch (const std::exception& e) {
            profile_compare_worker_active_ = false;
            profile_compare_session_.active = false;
            profile_compare_status_.armed = false;
            profile_compare_status_.error_message = std::string("启动 profile compare worker 失败: ") + e.what();
            return Result<void>::Failure(
                Shared::Types::Error(
                    ErrorCode::UNKNOWN_ERROR,
                    profile_compare_status_.error_message,
                    "ValveAdapter"));
        }

        SILIGEN_LOG_INFO("ArmProfileCompare: source_axis=" + std::to_string(compare_source_axis) +
                         ", cmp_channel=" + std::to_string(cmp_channel) +
                         ", business_trigger_count=" + std::to_string(request.expected_trigger_count) +
                         ", start_boundary_trigger_count=" +
                         std::to_string(request.start_boundary_trigger_count) +
                         ", future_compare_count=" +
                          std::to_string(request.compare_positions_pulse.size()) +
                         ", initial_buffer1_load=" + std::to_string(initial_chunk.buffer1.size()) +
                         ", initial_buffer2_load=" + std::to_string(initial_chunk.buffer2.size()) +
                         ", pulse_width_us=" + std::to_string(request.pulse_width_us) +
                         ", abs_position_flag=" + std::to_string(abs_position_flag) +
                         ", base_profile_position_pulse=" + std::to_string(current_profile_position_pulse));
        return Result<void>::Success();
    }
    catch (const std::exception& e) {
        ResetProfileCompareTrackingState();
        profile_compare_status_.error_message = std::string("Exception: ") + e.what();
        return Result<void>::Failure(
            Shared::Types::Error(ErrorCode::UNKNOWN_ERROR, profile_compare_status_.error_message, "ValveAdapter"));
    }
}

Result<void> ValveAdapter::DisarmProfileCompare() noexcept {
    try {
        StopProfileCompareWorker();
        std::lock_guard<std::mutex> lock(dispenser_mutex_);
        const int reset_result = ResetDispenserHardwareState("DisarmProfileCompare", false);
        if (reset_result != 0) {
            const auto error_message = CreateErrorMessage(reset_result, "DisarmProfileCompare");
            ResetProfileCompareTrackingState();
            profile_compare_status_.error_message = error_message;
            return Result<void>::Failure(
                Shared::Types::Error(ErrorCode::HARDWARE_ERROR, error_message, "ValveAdapter"));
        }

        ResetProfileCompareTrackingState();
        return Result<void>::Success();
    }
    catch (const std::exception& e) {
        ResetProfileCompareTrackingState();
        profile_compare_status_.error_message = std::string("Exception: ") + e.what();
        return Result<void>::Failure(
            Shared::Types::Error(ErrorCode::UNKNOWN_ERROR, profile_compare_status_.error_message, "ValveAdapter"));
    }
}

Result<ProfileCompareStatus> ValveAdapter::GetProfileCompareStatus() noexcept {
    try {
        std::lock_guard<std::mutex> lock(dispenser_mutex_);
        if (!profile_compare_status_.armed && !profile_compare_session_.active) {
            return Result<ProfileCompareStatus>::Success(profile_compare_status_);
        }

        // The background worker is already the single owner polling MC_CmpBufSts and
        // materializing the hardware snapshot into profile_compare_status_. Returning the
        // cached snapshot here avoids foreground/background contention on the board API while
        // a compare session is active.
        if (profile_compare_session_.active && profile_compare_worker_active_) {
            return Result<ProfileCompareStatus>::Success(profile_compare_status_);
        }

        unsigned short status = 0;
        unsigned short remain_data_1 = 0;
        unsigned short remain_data_2 = 0;
        unsigned short remain_space_1 = 0;
        unsigned short remain_space_2 = 0;
        const int result = hardware_->MC_CmpBufSts(
            &status,
            &remain_data_1,
            &remain_data_2,
            &remain_space_1,
            &remain_space_2);
        if (result != 0) {
            const auto error_message = CreateErrorMessage(result, "MC_CmpBufSts");
            profile_compare_status_.error_message = error_message;
            return Result<ProfileCompareStatus>::Failure(
                Shared::Types::Error(ErrorCode::HARDWARE_ERROR, error_message, "ValveAdapter"));
        }

        UpdateProfileCompareStatusFromHardwareSnapshot(
            status,
            remain_data_1,
            remain_data_2,
            remain_space_1,
            remain_space_2);
        if (profile_compare_session_.submitted_future_compare_count >=
                profile_compare_session_.compare_positions_pulse.size() &&
            remain_data_1 == 0U && remain_data_2 == 0U) {
            profile_compare_status_.armed = false;
            profile_compare_session_.active = false;
            profile_compare_worker_active_ = false;
        }
        profile_compare_status_.error_message.clear();
        return Result<ProfileCompareStatus>::Success(profile_compare_status_);
    }
    catch (const std::exception& e) {
        profile_compare_status_.error_message = std::string("Exception: ") + e.what();
        return Result<ProfileCompareStatus>::Failure(
            Shared::Types::Error(ErrorCode::UNKNOWN_ERROR, profile_compare_status_.error_message, "ValveAdapter"));
    }
}

bool ValveAdapter::ReadCoordinateSystemPositionPulse(short coordinate_system,
                                                     long& x_position_pulse,
                                                     long& y_position_pulse) noexcept {
    try {
        double positions[8] = {};
        const int result = hardware_->MC_GetCrdPos(coordinate_system, positions);
        if (result != 0) {
            SILIGEN_LOG_WARNING("ReadCoordinateSystemPositionPulse: MC_GetCrdPos 失败, result=" +
                                std::to_string(result));
            return false;
        }

        x_position_pulse = static_cast<long>(std::llround(positions[0]));
        y_position_pulse = static_cast<long>(std::llround(positions[1]));
        return true;
    } catch (...) {
        SILIGEN_LOG_WARNING("ReadCoordinateSystemPositionPulse: 读取坐标系位置时发生异常");
        return false;
    }
}

void ValveAdapter::RefreshTimedDispenserStateIfCompleted() noexcept {
    if (dispenser_run_mode_ != DispenserRunMode::Timed) {
        return;
    }
    if (dispenser_state_.status != DispenserValveStatus::Running &&
        dispenser_state_.status != DispenserValveStatus::Paused) {
        return;
    }
    UpdateDispenserProgress();
}

void ValveAdapter::UpdateDispenserProgress() {
    // 注意：此方法应在持有 dispenser_mutex_ 锁时调用

    if (dispenser_state_.totalCount == 0) {
        return;
    }

    if (dispenser_state_.completedCount > dispenser_state_.totalCount) {
        dispenser_state_.completedCount = dispenser_state_.totalCount;
    }

    dispenser_state_.remainingCount = dispenser_state_.totalCount - dispenser_state_.completedCount;
    dispenser_state_.progress =
        (static_cast<float>(dispenser_state_.completedCount) /
         static_cast<float>(dispenser_state_.totalCount)) * 100.0f;

    // 仅定时模式在计数完成时自动切换为空闲。路径触发模式由 StopDispenser 收口。
    if (dispenser_run_mode_ == DispenserRunMode::Timed &&
        dispenser_state_.completedCount >= dispenser_state_.totalCount) {
        dispenser_state_.status = DispenserValveStatus::Idle;
        dispenser_state_.progress = 100.0f;
        dispenser_state_.remainingCount = 0;
        dispenser_run_mode_ = DispenserRunMode::None;
    }
}

std::string ValveAdapter::CreateErrorMessage(int errorCode, const std::string& operation) {
    // 根据 MultiCard SDK 的错误码创建详细错误信息
    // 错误码定义来自 MultiCardCPP.h
    std::string msg = "[ValveAdapter] MultiCard API Error in " + operation + ": ";

    switch (errorCode) {
        // 成功（不应该调用此方法，但为完整性添加）
        case 0:   // MC_COM_SUCCESS
            msg += "操作成功（不应有此错误）";
            break;

        // 执行相关错误
        case 1:   // MC_COM_ERR_EXEC_FAIL
            msg += "执行失败（检测命令执行条件是否满足）";
            break;

        case 2:   // MC_COM_ERR_LICENSE_WRONG
            msg += "版本不支持该API（如有需要，联系厂家）";
            break;

        case 7:   // MC_COM_ERR_DATA_WORRY
            msg += "参数错误（检测参数是否合理）";
            break;

        // 通信相关错误
        case -1:  // MC_COM_ERR_SEND
            msg += "通讯失败（检查接线是否牢靠，必要时更换板卡）";
            break;

        case -6:  // MC_COM_ERR_CARD_OPEN_FAIL
            msg += "打开控制器失败（检查串口名/IP与端口配置，避免重复调用 MC_Open）";
            break;

        case -7:  // MC_COM_ERR_TIME_OUT
            msg += "运动控制器无响应（检测控制器是否连接、是否打开，必要时更换板卡）";
            break;

        case -8:  // MC_COM_ERR_COM_OPEN_FAIL
            msg += "串口打开失败（检查串口权限、占用或驱动）";
            break;

        // 特殊错误
        case -9999:
            msg += "调用异常（软件内部异常，请检查日志）";
            break;

        // 未知错误
        default:
            msg += "未知错误 (错误码=" + std::to_string(errorCode) + "，请查阅 SDK 文档)";
            break;
    }

    // 添加错误码以便调试
    msg += " [ErrorCode: " + std::to_string(errorCode) + "]";

    return msg;
}


} // namespace Adapters
} // namespace Infrastructure
} // namespace Siligen



