#include "MockIoControlService.h"

#include "runtime/configuration/InterlockConfigResolver.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "siligen/device/adapters/drivers/multicard/MockMultiCardWrapper.h"

#include <cstdint>

#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "MockIoControlService"

namespace {

constexpr short kGeneralPurposeInputGroup = MC_GPI;
constexpr short kHomeInputGroup = MC_HOME;
constexpr short kAxisXHomeBit = 0;
constexpr short kAxisYHomeBit = 1;

bool ReadBit(long raw, short bit) {
    return (raw & (1L << bit)) != 0;
}

void WriteBit(long& raw, short bit, bool enabled) {
    const long mask = (1L << bit);
    if (enabled) {
        raw |= mask;
        return;
    }
    raw &= ~mask;
}

bool ComputeEstopTriggered(long raw, short bit, bool active_low) {
    const bool raw_high = ReadBit(raw, bit);
    return active_low ? !raw_high : raw_high;
}

bool ComputeDoorOpen(long raw, short bit) {
    if (bit < 0) {
        return false;
    }
    return ReadBit(raw, bit);
}

bool RawLevelForEstop(bool estop_triggered, bool active_low) {
    return active_low ? !estop_triggered : estop_triggered;
}

}  // namespace

namespace Siligen::Adapters::Tcp {

using Shared::Types::Error;
using Shared::Types::ErrorCode;
using Shared::Types::Result;

MockIoControlService::MockIoControlService(
    std::string config_path,
    Siligen::Infrastructure::Hardware::MockMultiCardWrapper* mock_multicard,
    bool mock_mode_enabled)
    : config_path_(std::move(config_path)),
      mock_multicard_(mock_multicard),
      mock_mode_enabled_(mock_mode_enabled) {
    const auto interlock_resolution =
        Siligen::Infrastructure::Configuration::ResolveInterlockConfigFromIni(config_path_);
    estop_input_ = interlock_resolution.config.emergency_stop_input;
    estop_active_low_ = interlock_resolution.config.emergency_stop_active_low;
    door_input_ = interlock_resolution.config.safety_door_input;
}

Result<long> MockIoControlService::ReadInputRaw(short group) const {
    if (!mock_multicard_) {
        return Result<long>::Failure(
            Error(ErrorCode::INVALID_STATE, "mock.io.set requires a mock multicard instance", "MockIoControlService"));
    }

    long raw = 0;
    const int ret = mock_multicard_->MC_GetDiRaw(group, &raw);
    if (ret != 0) {
        return Result<long>::Failure(
            Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                  "mock.io.set failed to read digital input group " + std::to_string(group) +
                      ", error=" + std::to_string(ret),
                  "MockIoControlService"));
    }
    return Result<long>::Success(raw);
}

Result<void> MockIoControlService::WriteInputRaw(short group, long value) const {
    if (!mock_multicard_) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "mock.io.set requires a mock multicard instance", "MockIoControlService"));
    }

    mock_multicard_->SetDigitalInputRaw(group, value);
    return Result<void>::Success();
}

Result<MockIoState> MockIoControlService::ReadState() const {
    if (!mock_mode_enabled_) {
        return Result<MockIoState>::Failure(
            Error(ErrorCode::INVALID_STATE,
                  "mock.io.set is only available when Hardware.mode=Mock",
                  "MockIoControlService"));
    }

    auto gpi_result = ReadInputRaw(kGeneralPurposeInputGroup);
    if (gpi_result.IsError()) {
        return Result<MockIoState>::Failure(gpi_result.GetError());
    }

    auto home_result = ReadInputRaw(kHomeInputGroup);
    if (home_result.IsError()) {
        return Result<MockIoState>::Failure(home_result.GetError());
    }

    MockIoState state;
    state.estop = ComputeEstopTriggered(gpi_result.Value(), estop_input_, estop_active_low_);
    state.door = ComputeDoorOpen(gpi_result.Value(), door_input_);
    state.limit_x_neg = ReadBit(home_result.Value(), kAxisXHomeBit);
    state.limit_y_neg = ReadBit(home_result.Value(), kAxisYHomeBit);
    return Result<MockIoState>::Success(state);
}

Result<MockIoState> MockIoControlService::Apply(const MockIoUpdateRequest& request) const {
    if (!mock_mode_enabled_) {
        return Result<MockIoState>::Failure(
            Error(ErrorCode::INVALID_STATE,
                  "mock.io.set is only available when Hardware.mode=Mock",
                  "MockIoControlService"));
    }
    if (!mock_multicard_) {
        return Result<MockIoState>::Failure(
            Error(ErrorCode::INVALID_STATE, "mock.io.set requires a mock multicard instance", "MockIoControlService"));
    }
    if (request.door.has_value() && door_input_ < 0) {
        return Result<MockIoState>::Failure(
            Error(ErrorCode::INVALID_CONFIG_VALUE,
                  "mock.io.set cannot drive door because [Interlock].safety_door_input is disabled",
                  "MockIoControlService"));
    }

    auto gpi_result = ReadInputRaw(kGeneralPurposeInputGroup);
    if (gpi_result.IsError()) {
        return Result<MockIoState>::Failure(gpi_result.GetError());
    }
    auto home_result = ReadInputRaw(kHomeInputGroup);
    if (home_result.IsError()) {
        return Result<MockIoState>::Failure(home_result.GetError());
    }

    long gpi_raw = gpi_result.Value();
    long home_raw = home_result.Value();

    if (request.estop.has_value()) {
        WriteBit(gpi_raw, estop_input_, RawLevelForEstop(request.estop.value(), estop_active_low_));
    }
    if (request.door.has_value()) {
        WriteBit(gpi_raw, door_input_, request.door.value());
    }
    if (request.limit_x_neg.has_value()) {
        WriteBit(home_raw, kAxisXHomeBit, request.limit_x_neg.value());
    }
    if (request.limit_y_neg.has_value()) {
        WriteBit(home_raw, kAxisYHomeBit, request.limit_y_neg.value());
    }

    auto write_gpi_result = WriteInputRaw(kGeneralPurposeInputGroup, gpi_raw);
    if (write_gpi_result.IsError()) {
        return Result<MockIoState>::Failure(write_gpi_result.GetError());
    }
    auto write_home_result = WriteInputRaw(kHomeInputGroup, home_raw);
    if (write_home_result.IsError()) {
        return Result<MockIoState>::Failure(write_home_result.GetError());
    }

    auto state_result = ReadState();
    if (state_result.IsSuccess()) {
        const auto& state = state_result.Value();
        SILIGEN_LOG_INFO_FMT_HELPER(
            "mock.io.set applied estop=%d door=%d limit_x_neg=%d limit_y_neg=%d config=%s",
            state.estop,
            state.door,
            state.limit_x_neg,
            state.limit_y_neg,
            config_path_.c_str());
    }
    return state_result;
}

}  // namespace Siligen::Adapters::Tcp
