#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "services/dispensing/DispensingProcessService.h"
#include "runtime_execution/contracts/motion/IInterpolationPort.h"
#include "runtime_execution/contracts/motion/IMotionStatePort.h"
#include "shared/types/Error.h"

namespace {

using Siligen::RuntimeExecution::Application::Services::Dispensing::DispensingProcessService;
using Siligen::Domain::Dispensing::Ports::IDispensingExecutionObserver;
using Siligen::Domain::Dispensing::Ports::DispenserValveParams;
using Siligen::Domain::Dispensing::Ports::DispenserValveState;
using Siligen::Domain::Dispensing::Ports::DispenserValveStatus;
using Siligen::Domain::Dispensing::Ports::IProfileComparePort;
using Siligen::Domain::Dispensing::Ports::IValvePort;
using Siligen::Domain::Dispensing::Ports::PositionTriggeredDispenserParams;
using Siligen::Domain::Dispensing::Ports::ProfileCompareArmRequest;
using Siligen::Domain::Dispensing::Ports::ProfileCompareStatus;
using Siligen::Domain::Dispensing::Ports::SupplyValveState;
using Siligen::Domain::Dispensing::Ports::SupplyValveStatusDetail;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionOptions;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionPlan;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionReport;
using Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeParams;
using Siligen::Domain::Motion::Ports::CoordinateSystemConfig;
using Siligen::Domain::Motion::Ports::CoordinateSystemState;
using Siligen::Domain::Motion::Ports::CoordinateSystemStatus;
using Siligen::Domain::Motion::Ports::IInterpolationPort;
using Siligen::Domain::Motion::Ports::IMotionStatePort;
using Siligen::Domain::Motion::Ports::InterpolationData;
using Siligen::Domain::Motion::Ports::InterpolationType;
using Siligen::Domain::Motion::Ports::MotionState;
using Siligen::Domain::Motion::Ports::MotionStatus;
using Siligen::Device::Contracts::Commands::DeviceConnection;
using Siligen::Device::Contracts::Ports::DeviceConnectionPort;
using Siligen::Device::Contracts::State::DeviceConnectionSnapshot;
using Siligen::Device::Contracts::State::DeviceConnectionState;
using Siligen::Device::Contracts::State::HeartbeatSnapshot;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using int16 = std::int16_t;
using int32 = std::int32_t;
using uint32 = std::uint32_t;

template <typename Tag, typename Tag::type Member>
struct PrivateMemberAccessor {
    friend typename Tag::type GetPrivateMember(Tag) {
        return Member;
    }
};

struct WaitForMotionCompleteTag {
    using type = Result<void> (DispensingProcessService::*)(int32,
                                                            std::atomic<bool>*,
                                                            std::atomic<bool>*,
                                                            std::atomic<bool>*,
                                                            const Point2D*,
                                                            float32,
                                                            uint32,
                                                            uint32,
                                                            bool,
                                                            IDispensingExecutionObserver*,
                                                            const std::function<Result<void>(const ProfileCompareStatus&)>&) noexcept;
    friend type GetPrivateMember(WaitForMotionCompleteTag);
};

struct ExecutePlanInternalTag {
    using type = Result<DispensingExecutionReport> (DispensingProcessService::*)(const DispensingExecutionPlan&,
                                                                                 float32,
                                                                                 const DispensingRuntimeParams&,
                                                                                 const DispensingExecutionOptions&,
                                                                                 std::atomic<bool>*,
                                                                                 std::atomic<bool>*,
                                                                                 std::atomic<bool>*,
                                                                                 IDispensingExecutionObserver*)
        noexcept;
    friend type GetPrivateMember(ExecutePlanInternalTag);
};

struct ConfigureCoordinateSystemTag {
    using type = Result<void> (DispensingProcessService::*)(const DispensingRuntimeParams&) noexcept;
    friend type GetPrivateMember(ConfigureCoordinateSystemTag);
};

struct ClearInterpolationBufferForFormalPathTag {
    using type =
        Result<void> (DispensingProcessService::*)(std::atomic<bool>*, std::atomic<bool>*, std::atomic<bool>*)
            noexcept;
    friend type GetPrivateMember(ClearInterpolationBufferForFormalPathTag);
};

template struct PrivateMemberAccessor<
    WaitForMotionCompleteTag,
    &DispensingProcessService::WaitForMotionComplete>;
template struct PrivateMemberAccessor<
    ExecutePlanInternalTag,
    &DispensingProcessService::ExecutePlanInternal>;
template struct PrivateMemberAccessor<
    ConfigureCoordinateSystemTag,
    &DispensingProcessService::ConfigureCoordinateSystem>;
template struct PrivateMemberAccessor<
    ClearInterpolationBufferForFormalPathTag,
    &DispensingProcessService::ClearInterpolationBufferForFormalPath>;

template <typename T>
Result<T> NotImplemented(const char* method) {
    return Result<T>::Failure(Error(ErrorCode::NOT_IMPLEMENTED, method, "DispensingProcessServiceWaitTest"));
}

Result<void> NotImplementedVoid(const char* method) {
    return Result<void>::Failure(Error(ErrorCode::NOT_IMPLEMENTED, method, "DispensingProcessServiceWaitTest"));
}

class FakeInterpolationPort final : public IInterpolationPort {
   public:
    Result<void> ConfigureCoordinateSystem(int16, const CoordinateSystemConfig& config) override {
        ++configure_calls;
        last_config = config;
        return Result<void>::Success();
    }

    Result<void> AddInterpolationData(int16, const InterpolationData& data) override {
        ++add_calls;
        added_segments.push_back(data);
        add_history.push_back(data);
        return Result<void>::Success();
    }

    Result<void> ClearInterpolationBuffer(int16) override {
        ++clear_calls;
        const auto clear_status = PeekCurrentStatus();
        if (fail_clear_when_run_bit_set &&
            clear_calls >= clear_fail_from_call &&
            (clear_status.raw_status_word & kCrdStatusProgRun) != 0) {
            return Result<void>::Failure(
                Error(ErrorCode::HARDWARE_ERROR,
                      "ClearInterpolationBuffer: simulated latched RUN bit",
                      "DispensingProcessServiceWaitTest"));
        }
        if (!scripted_clear_failures.empty()) {
            const auto index = scripted_clear_failure_cursor < scripted_clear_failures.size()
                                   ? scripted_clear_failure_cursor
                                   : scripted_clear_failures.size() - 1U;
            const bool should_fail = scripted_clear_failures[index];
            if (scripted_clear_failure_cursor + 1U < scripted_clear_failures.size()) {
                ++scripted_clear_failure_cursor;
            }
            if (should_fail) {
                return Result<void>::Failure(
                    Error(ErrorCode::HARDWARE_ERROR,
                          scripted_clear_failure_message,
                          "DispensingProcessServiceWaitTest"));
            }
        }
        added_segments.clear();
        return Result<void>::Success();
    }

    Result<void> FlushInterpolationData(int16) override {
        ++flush_calls;
        return Result<void>::Success();
    }

    Result<void> StartCoordinateSystemMotion(uint32) override {
        ++start_calls;
        status.state = CoordinateSystemState::IDLE;
        status.is_moving = false;
        status.remaining_segments = 0U;
        status.raw_segment = 0;
        return Result<void>::Success();
    }

    Result<void> StopCoordinateSystemMotion(uint32) override {
        ++stop_calls;
        return Result<void>::Success();
    }

    Result<void> SetCoordinateSystemVelocityOverride(int16, float32) override {
        return Result<void>::Success();
    }

    Result<void> EnableCoordinateSystemSCurve(int16, float32) override {
        return NotImplementedVoid("EnableCoordinateSystemSCurve");
    }

    Result<void> DisableCoordinateSystemSCurve(int16) override {
        return NotImplementedVoid("DisableCoordinateSystemSCurve");
    }

    Result<void> SetConstLinearVelocityMode(int16, bool, uint32) override {
        return NotImplementedVoid("SetConstLinearVelocityMode");
    }

    Result<uint32> GetInterpolationBufferSpace(int16) const override {
        return Result<uint32>::Success(buffer_space);
    }

    Result<uint32> GetLookAheadBufferSpace(int16) const override {
        return Result<uint32>::Success(lookahead_space);
    }

    Result<CoordinateSystemStatus> GetCoordinateSystemStatus(int16) const override {
        ++status_reads;
        if (!status_sequence.empty()) {
            const auto index =
                status_sequence_cursor < status_sequence.size() ? status_sequence_cursor : status_sequence.size() - 1U;
            const auto value = status_sequence[index];
            if (status_sequence_cursor + 1U < status_sequence.size()) {
                ++status_sequence_cursor;
            }
            return Result<CoordinateSystemStatus>::Success(value);
        }
        return Result<CoordinateSystemStatus>::Success(status);
    }

    CoordinateSystemStatus PeekCurrentStatus() const {
        if (!status_sequence.empty()) {
            const auto index =
                status_sequence_cursor < status_sequence.size() ? status_sequence_cursor : status_sequence.size() - 1U;
            return status_sequence[index];
        }
        return status;
    }

    CoordinateSystemStatus status{
        CoordinateSystemState::MOVING,
        true,
        1U,
        0.0f,
        0,
        1,
        0};
    uint32 buffer_space = 64U;
    uint32 lookahead_space = 0U;
    int configure_calls = 0;
    int add_calls = 0;
    int clear_calls = 0;
    int flush_calls = 0;
    int start_calls = 0;
    int stop_calls = 0;
    mutable int status_reads = 0;
    mutable std::size_t status_sequence_cursor = 0;
    CoordinateSystemConfig last_config{};
    std::vector<CoordinateSystemStatus> status_sequence{};
    std::vector<InterpolationData> added_segments{};
    std::vector<InterpolationData> add_history{};
    std::vector<bool> scripted_clear_failures{};
    bool fail_clear_when_run_bit_set = false;
    int clear_fail_from_call = 2;
    std::string scripted_clear_failure_message = "ClearInterpolationBuffer: simulated scripted failure";

   private:
    static constexpr int32 kCrdStatusProgRun = 0x00000001;
    mutable std::size_t scripted_clear_failure_cursor = 0;
};

class SequencedMotionStatePort final : public IMotionStatePort {
   public:
    explicit SequencedMotionStatePort(std::vector<Point2D> positions_in) : positions(std::move(positions_in)) {}

    Result<Point2D> GetCurrentPosition() const override {
        if (positions.empty()) {
            return Result<Point2D>::Success(Point2D{});
        }
        const auto index = current_index < positions.size() ? current_index : positions.size() - 1;
        const auto value = positions[index];
        if (current_index + 1 < positions.size()) {
            ++current_index;
        }
        last_position = value;
        return Result<Point2D>::Success(value);
    }

    Result<float32> GetAxisPosition(LogicalAxisId axis) const override {
        if (axis == LogicalAxisId::X) {
            return Result<float32>::Success(last_position.x);
        }
        if (axis == LogicalAxisId::Y) {
            return Result<float32>::Success(last_position.y);
        }
        return Result<float32>::Success(0.0f);
    }

    Result<float32> GetAxisVelocity(LogicalAxisId) const override {
        return Result<float32>::Success(0.0f);
    }

    Result<MotionStatus> GetAxisStatus(LogicalAxisId) const override {
        MotionStatus status;
        status.state = MotionState::IDLE;
        status.position = last_position;
        status.velocity = 0.0f;
        return Result<MotionStatus>::Success(status);
    }

    Result<bool> IsAxisMoving(LogicalAxisId) const override {
        return Result<bool>::Success(false);
    }

    Result<bool> IsAxisInPosition(LogicalAxisId) const override {
        return Result<bool>::Success(true);
    }

    Result<std::vector<MotionStatus>> GetAllAxesStatus() const override {
        return Result<std::vector<MotionStatus>>::Success({});
    }

    mutable std::size_t current_index = 0;
    mutable Point2D last_position{};
    std::vector<Point2D> positions{};
};

class FakeHardwareConnectionPort final : public DeviceConnectionPort {
   public:
    Result<void> Connect(const DeviceConnection&) override { return Result<void>::Success(); }

    Result<void> Disconnect() override { return Result<void>::Success(); }

    Result<DeviceConnectionSnapshot> ReadConnection() const override {
        DeviceConnectionSnapshot snapshot;
        snapshot.state = connected ? DeviceConnectionState::Connected : DeviceConnectionState::Disconnected;
        return Result<DeviceConnectionSnapshot>::Success(snapshot);
    }

    bool IsConnected() const override { return connected; }

    Result<void> Reconnect() override { return Result<void>::Success(); }

    void SetConnectionStateCallback(std::function<void(const DeviceConnectionSnapshot&)>) override {}

    Result<void> StartStatusMonitoring(std::uint32_t) override { return Result<void>::Success(); }

    void StopStatusMonitoring() override {}

    std::string GetLastError() const override { return {}; }

    void ClearError() override {}

    Result<void> StartHeartbeat(const HeartbeatSnapshot&) override { return Result<void>::Success(); }

    void StopHeartbeat() override {}

    HeartbeatSnapshot ReadHeartbeat() const override { return {}; }

    Result<bool> Ping() const override { return Result<bool>::Success(connected); }

    bool connected = true;
};

class DelayedProfileCompareValvePort final : public IValvePort,
                                            public IProfileComparePort {
   public:
    Result<DispenserValveState> StartDispenser(const DispenserValveParams&) noexcept override {
        return Result<DispenserValveState>::Success(dispenser_state);
    }

    Result<DispenserValveState> OpenDispenser() noexcept override {
        return Result<DispenserValveState>::Success(dispenser_state);
    }

    Result<void> CloseDispenser() noexcept override {
        return Result<void>::Success();
    }

    Result<DispenserValveState> StartPositionTriggeredDispenser(
        const PositionTriggeredDispenserParams&) noexcept override {
        return Result<DispenserValveState>::Success(dispenser_state);
    }

    Result<void> StopDispenser() noexcept override {
        return Result<void>::Success();
    }

    Result<void> PauseDispenser() noexcept override {
        return Result<void>::Success();
    }

    Result<void> ResumeDispenser() noexcept override {
        return Result<void>::Success();
    }

    Result<DispenserValveState> GetDispenserStatus() noexcept override {
        return Result<DispenserValveState>::Success(dispenser_state);
    }

    Result<SupplyValveState> OpenSupply() noexcept override {
        return Result<SupplyValveState>::Success(SupplyValveState::Open);
    }

    Result<SupplyValveState> CloseSupply() noexcept override {
        return Result<SupplyValveState>::Success(SupplyValveState::Closed);
    }

    Result<SupplyValveStatusDetail> GetSupplyStatus() noexcept override {
        return Result<SupplyValveStatusDetail>::Success({});
    }

    Result<void> ArmProfileCompare(const ProfileCompareArmRequest& request) noexcept override {
        profile_compare_status = {};
        profile_compare_status.armed = !request.compare_positions_pulse.empty();
        profile_compare_status.expected_trigger_count = request.expected_trigger_count;
        profile_compare_status.completed_trigger_count = request.start_boundary_trigger_count;
        profile_compare_status.remaining_trigger_count =
            request.expected_trigger_count - request.start_boundary_trigger_count;
        remaining_incomplete_status_reads = incomplete_status_reads_before_complete;
        return Result<void>::Success();
    }

    Result<void> DisarmProfileCompare() noexcept override {
        profile_compare_status.armed = false;
        profile_compare_status.remaining_trigger_count = 0U;
        profile_compare_status.completed_trigger_count = profile_compare_status.expected_trigger_count;
        return Result<void>::Success();
    }

    Result<ProfileCompareStatus> GetProfileCompareStatus() noexcept override {
        ++status_reads;
        if (profile_compare_status.armed) {
            if (remaining_incomplete_status_reads > 0) {
                --remaining_incomplete_status_reads;
            } else {
                profile_compare_status.armed = false;
                profile_compare_status.completed_trigger_count = profile_compare_status.expected_trigger_count;
                profile_compare_status.remaining_trigger_count = 0U;
            }
        }
        return Result<ProfileCompareStatus>::Success(profile_compare_status);
    }

    int incomplete_status_reads_before_complete = 0;
    int remaining_incomplete_status_reads = 0;
    int status_reads = 0;
    ProfileCompareStatus profile_compare_status{};
    DispenserValveState dispenser_state{DispenserValveStatus::Idle, 0U, 0U, 0U, 0.0f, std::nullopt, {}};
};

DispensingExecutionPlan BuildLinearExecutionPlan() {
    DispensingExecutionPlan plan;

    InterpolationData first_segment;
    first_segment.type = InterpolationType::LINEAR;
    first_segment.positions = {5.0f, 0.0f};
    first_segment.velocity = 20.0f;
    first_segment.acceleration = 100.0f;
    first_segment.end_velocity = 20.0f;

    InterpolationData second_segment;
    second_segment.type = InterpolationType::LINEAR;
    second_segment.positions = {10.0f, 0.0f};
    second_segment.velocity = 20.0f;
    second_segment.acceleration = 100.0f;
    second_segment.end_velocity = 0.0f;

    plan.interpolation_segments = {first_segment, second_segment};

    const std::vector<Point2D> preview_positions{
        Point2D{0.0f, 0.0f},
        Point2D{2.5f, 0.0f},
        Point2D{5.0f, 0.0f},
        Point2D{7.5f, 0.0f},
        Point2D{10.0f, 0.0f}};
    plan.interpolation_points.reserve(preview_positions.size());
    for (std::size_t index = 0; index < preview_positions.size(); ++index) {
        Siligen::TrajectoryPoint point;
        point.position = preview_positions[index];
        point.sequence_id = static_cast<uint32>(index);
        point.timestamp = static_cast<float32>(index) * 0.1f;
        point.velocity = 20.0f;
        plan.interpolation_points.push_back(point);
    }

    plan.total_length_mm = 10.0f;
    return plan;
}

DispensingRuntimeParams BuildRuntimeParams() {
    DispensingRuntimeParams params;
    params.dispensing_velocity = 20.0f;
    params.acceleration = 100.0f;
    params.pulse_per_mm = 200.0f;
    params.dispenser_interval_ms = 100U;
    params.dispenser_duration_ms = 50U;
    params.trigger_spatial_interval_mm = 2.0f;
    return params;
}

DispensingExecutionOptions BuildExecutionOptions() {
    DispensingExecutionOptions options;
    options.dispense_enabled = false;
    options.use_hardware_trigger = false;
    options.guard_decision.allow_motion = true;
    options.guard_decision.allow_valve = false;
    options.guard_decision.allow_supply = false;
    options.guard_decision.allow_cmp = false;
    return options;
}

TEST(DispensingProcessServiceWaitForMotionCompleteTest, StablePositionAwayFromTargetTimesOut) {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(
        std::vector<Point2D>{Point2D{0.0f, 0.0f}, Point2D{92.305f, 0.0f}});
    DispensingProcessService service(nullptr,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);

    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    Point2D final_target{100.0f, 100.0f};
    const auto wait_for_motion_complete = GetPrivateMember(WaitForMotionCompleteTag{});

    auto result = (service.*wait_for_motion_complete)(
        180,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        &final_target,
        1.0f,
        9U,
        0U,
        false,
        nullptr,
        nullptr);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::MOTION_TIMEOUT);
}

TEST(DispensingProcessServiceWaitForMotionCompleteTest, NearTargetLowVelocityFallsBackToSuccess) {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    interpolation_port->status.state = CoordinateSystemState::IDLE;
    interpolation_port->status.is_moving = false;
    interpolation_port->status.remaining_segments = 0U;
    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(
        std::vector<Point2D>{Point2D{0.0f, 0.0f}, Point2D{99.7f, 99.8f}, Point2D{99.7f, 99.8f}, Point2D{99.7f, 99.8f},
                             Point2D{99.7f, 99.8f}, Point2D{99.7f, 99.8f}, Point2D{99.7f, 99.8f}});
    DispensingProcessService service(nullptr,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);

    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    Point2D final_target{100.0f, 100.0f};
    const auto wait_for_motion_complete = GetPrivateMember(WaitForMotionCompleteTag{});

    auto result = (service.*wait_for_motion_complete)(
        800,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        &final_target,
        1.0f,
        9U,
        0U,
        false,
        nullptr,
        nullptr);

    ASSERT_TRUE(result.IsSuccess());
}

TEST(DispensingProcessServiceWaitForMotionCompleteTest, FifoFinishedWithLatchedRunBitFallsBackToSuccess) {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    interpolation_port->status.state = CoordinateSystemState::MOVING;
    interpolation_port->status.is_moving = true;
    interpolation_port->status.remaining_segments = std::numeric_limits<uint32>::max();
    interpolation_port->status.current_velocity = 0.0f;
    interpolation_port->status.raw_status_word = 0x11;
    interpolation_port->status.raw_segment = -858993460;
    interpolation_port->status.mc_status_ret = 0;

    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(
        std::vector<Point2D>{Point2D{0.0f, 0.0f}, Point2D{100.0f, 100.0f}, Point2D{100.0f, 100.0f},
                             Point2D{100.0f, 100.0f}, Point2D{100.0f, 100.0f}, Point2D{100.0f, 100.0f},
                             Point2D{100.0f, 100.0f}});
    DispensingProcessService service(nullptr,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);

    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    Point2D final_target{100.0f, 100.0f};
    const auto wait_for_motion_complete = GetPrivateMember(WaitForMotionCompleteTag{});

    auto result = (service.*wait_for_motion_complete)(
        800,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        &final_target,
        1.0f,
        9U,
        0U,
        false,
        nullptr,
        nullptr);

    ASSERT_TRUE(result.IsSuccess());
}

TEST(DispensingProcessServiceWaitForMotionCompleteTest, IdleCoordinateSystemAwayFromTargetStillTimesOut) {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    interpolation_port->status.state = CoordinateSystemState::IDLE;
    interpolation_port->status.is_moving = false;
    interpolation_port->status.remaining_segments = 0U;

    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(
        std::vector<Point2D>{Point2D{0.0f, 0.0f}, Point2D{92.305f, 0.0f}, Point2D{92.305f, 0.0f},
                             Point2D{92.305f, 0.0f}, Point2D{92.305f, 0.0f}, Point2D{92.305f, 0.0f}});
    DispensingProcessService service(nullptr,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);

    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    Point2D final_target{100.0f, 100.0f};
    const auto wait_for_motion_complete = GetPrivateMember(WaitForMotionCompleteTag{});

    auto result = (service.*wait_for_motion_complete)(
        180,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        &final_target,
        1.0f,
        9U,
        0U,
        false,
        nullptr,
        nullptr);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::MOTION_TIMEOUT);
}

TEST(DispensingProcessServiceWaitForMotionCompleteTest,
     MotionTerminalWaitsForDelayedProfileCompareCompletionWithinBudget) {
    auto valve_port = std::make_shared<DelayedProfileCompareValvePort>();
    valve_port->incomplete_status_reads_before_complete = 8;
    ProfileCompareArmRequest request{};
    request.compare_source_axis = 1;
    request.expected_trigger_count = 3U;
    request.start_boundary_trigger_count = 1U;
    request.compare_positions_pulse = {100L, 200L};
    request.pulse_width_us = 2000U;
    request.start_level = 0;
    ASSERT_TRUE(valve_port->ArmProfileCompare(request).IsSuccess());

    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    interpolation_port->status.state = CoordinateSystemState::IDLE;
    interpolation_port->status.is_moving = false;
    interpolation_port->status.remaining_segments = 0U;
    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(
        std::vector<Point2D>{Point2D{0.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f}});
    DispensingProcessService service(valve_port,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);

    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    Point2D final_target{10.0f, 0.0f};
    const auto wait_for_motion_complete = GetPrivateMember(WaitForMotionCompleteTag{});

    auto result = (service.*wait_for_motion_complete)(
        1200,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        &final_target,
        1.0f,
        9U,
        request.expected_trigger_count,
        false,
        nullptr,
        nullptr);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_GE(valve_port->status_reads, 9);
}

TEST(DispensingProcessServiceWaitForMotionCompleteTest,
     MotionTerminalProfileCompareTimeoutStillReturnsCompareFailure) {
    auto valve_port = std::make_shared<DelayedProfileCompareValvePort>();
    valve_port->incomplete_status_reads_before_complete = 100;
    ProfileCompareArmRequest request{};
    request.compare_source_axis = 1;
    request.expected_trigger_count = 3U;
    request.start_boundary_trigger_count = 1U;
    request.compare_positions_pulse = {100L, 200L};
    request.pulse_width_us = 2000U;
    request.start_level = 0;
    ASSERT_TRUE(valve_port->ArmProfileCompare(request).IsSuccess());

    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    interpolation_port->status.state = CoordinateSystemState::IDLE;
    interpolation_port->status.is_moving = false;
    interpolation_port->status.remaining_segments = 0U;
    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(
        std::vector<Point2D>{Point2D{0.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f}});
    DispensingProcessService service(valve_port,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);

    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    Point2D final_target{10.0f, 0.0f};
    const auto wait_for_motion_complete = GetPrivateMember(WaitForMotionCompleteTag{});

    auto result = (service.*wait_for_motion_complete)(
        320,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        &final_target,
        1.0f,
        9U,
        request.expected_trigger_count,
        false,
        nullptr,
        nullptr);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::CMP_TRIGGER_SETUP_FAILED);
    EXPECT_EQ(result.GetError().GetMessage(), "profile_compare 未完成全部触发点");
}

TEST(DispensingProcessServiceWaitForMotionCompleteTest, ConfigureCoordinateSystemPinsDispensingCyclesToMachineZero) {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    DispensingProcessService service(nullptr,
                                     interpolation_port,
                                     nullptr,
                                     nullptr,
                                     nullptr);
    DispensingRuntimeParams params;
    params.dispensing_velocity = 20.0f;
    params.acceleration = 100.0f;

    const auto configure_coordinate_system = GetPrivateMember(ConfigureCoordinateSystemTag{});
    const auto result = (service.*configure_coordinate_system)(params);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(interpolation_port->configure_calls, 1);
    EXPECT_FLOAT_EQ(interpolation_port->last_config.max_velocity, 20.0f);
    EXPECT_FALSE(interpolation_port->last_config.use_current_planned_position_as_origin);
}

TEST(DispensingProcessServiceWaitForMotionCompleteTest, ExecutePlanInternalKeepsDispatchOrderWithPreviewTraceEnabled) {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    interpolation_port->status_sequence = {
        CoordinateSystemStatus{CoordinateSystemState::IDLE, false, 0U, 0.0f, 0x11, 1, 0}};
    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(
        std::vector<Point2D>{Point2D{0.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f}});
    DispensingProcessService service(nullptr,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);

    auto plan = BuildLinearExecutionPlan();
    auto params = BuildRuntimeParams();
    auto options = BuildExecutionOptions();
    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    const auto execute_plan_internal = GetPrivateMember(ExecutePlanInternalTag{});

    const auto result = (service.*execute_plan_internal)(
        plan,
        0.0f,
        params,
        options,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        nullptr);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(result.Value().executed_segments, 2U);
    EXPECT_FLOAT_EQ(result.Value().total_distance, 10.0f);
    EXPECT_EQ(interpolation_port->clear_calls, 1);
    EXPECT_EQ(interpolation_port->add_calls, 2);
    EXPECT_EQ(interpolation_port->flush_calls, 1);
    EXPECT_EQ(interpolation_port->start_calls, 1);
    ASSERT_EQ(interpolation_port->added_segments.size(), 2U);
    ASSERT_GE(interpolation_port->added_segments[0].positions.size(), 2U);
    ASSERT_GE(interpolation_port->added_segments[1].positions.size(), 2U);
    EXPECT_FLOAT_EQ(interpolation_port->added_segments[0].positions[0], 5.0f);
    EXPECT_FLOAT_EQ(interpolation_port->added_segments[1].positions[0], 10.0f);
}

TEST(DispensingProcessServiceWaitForMotionCompleteTest, ExecutePlanInternalPrePositionsToProgramStartWhenCurrentPositionDrifts) {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(
        std::vector<Point2D>{Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{0.0f, 0.0f},
                             Point2D{0.0f, 0.0f},
                             Point2D{0.0f, 0.0f},
                             Point2D{0.0f, 0.0f},
                             Point2D{0.0f, 0.0f},
                             Point2D{0.0f, 0.0f},
                             Point2D{0.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f}});
    DispensingProcessService service(nullptr,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);

    auto plan = BuildLinearExecutionPlan();
    auto params = BuildRuntimeParams();
    auto options = BuildExecutionOptions();
    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    const auto execute_plan_internal = GetPrivateMember(ExecutePlanInternalTag{});

    const auto result = (service.*execute_plan_internal)(
        plan,
        0.0f,
        params,
        options,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        nullptr);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(interpolation_port->clear_calls, 2);
    EXPECT_EQ(interpolation_port->add_calls, 3);
    EXPECT_EQ(interpolation_port->flush_calls, 2);
    EXPECT_EQ(interpolation_port->start_calls, 2);
    ASSERT_EQ(interpolation_port->add_history.size(), 3U);
    ASSERT_GE(interpolation_port->add_history[0].positions.size(), 2U);
    EXPECT_FLOAT_EQ(interpolation_port->add_history[0].positions[0], 0.0f);
    EXPECT_FLOAT_EQ(interpolation_port->add_history[0].positions[1], 0.0f);
    EXPECT_FLOAT_EQ(interpolation_port->add_history[0].velocity, 20.0f);
    EXPECT_FLOAT_EQ(interpolation_port->add_history[1].positions[0], 5.0f);
    EXPECT_FLOAT_EQ(interpolation_port->add_history[2].positions[0], 10.0f);
}

TEST(DispensingProcessServiceWaitForMotionCompleteTest,
     ExecuteProcessUsesAuthorityEstimatedTimeForFinalCompletionBudget) {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    interpolation_port->status_sequence.reserve(122);
    for (int index = 0; index < 6; ++index) {
        interpolation_port->status_sequence.push_back(
            CoordinateSystemStatus{CoordinateSystemState::IDLE, false, 0U, 0.0f, 0x10, 1, 0});
    }
    for (int index = 0; index < 107; ++index) {
        interpolation_port->status_sequence.push_back(
            CoordinateSystemStatus{CoordinateSystemState::MOVING, true, 1U, 1.0f, 0x01, 1, 1});
    }
    for (int index = 0; index < 8; ++index) {
        interpolation_port->status_sequence.push_back(
            CoordinateSystemStatus{CoordinateSystemState::IDLE, false, 0U, 0.0f, 0x10, 1, 0});
    }

    std::vector<Point2D> positions;
    positions.reserve(117);
    positions.push_back(Point2D{0.0f, 0.0f});
    positions.push_back(Point2D{0.0f, 0.0f});
    for (int index = 0; index < 107; ++index) {
        const auto ratio = static_cast<float>(index + 1) / 107.0f;
        positions.push_back(Point2D{9.8f * ratio, 0.0f});
    }
    for (int index = 0; index < 8; ++index) {
        positions.push_back(Point2D{10.0f, 0.0f});
    }
    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(std::move(positions));
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    DispensingProcessService service(nullptr,
                                     interpolation_port,
                                     motion_state_port,
                                     connection_port,
                                     nullptr);

    auto plan = BuildLinearExecutionPlan();
    plan.motion_trajectory.total_time = 0.0f;
    plan.interpolation_points.back().timestamp = 0.4f;

    Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated execution_package;
    execution_package.execution_plan = plan;
    execution_package.total_length_mm = plan.total_length_mm;
    // Keep the authority estimate above this synthetic ~7s wait path so the test
    // verifies authority-budget forwarding instead of timing out on fixture latency.
    execution_package.execution_nominal_time_s = 3.0f;

    auto params = BuildRuntimeParams();
    auto options = BuildExecutionOptions();
    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};

    const auto result = service.ExecuteProcess(
        execution_package,
        params,
        options,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        nullptr);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(result.Value().executed_segments, 2U);
}

TEST(DispensingProcessServiceWaitForMotionCompleteTest,
     ClearInterpolationBufferForFormalPathRetriesSettledClearUntilSuccess) {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    interpolation_port->status_sequence = {
        CoordinateSystemStatus{CoordinateSystemState::IDLE, false, 0U, 0.0f, 0x11, 1, 0}};
    interpolation_port->scripted_clear_failures = {true, true, true, false};
    DispensingProcessService service(nullptr,
                                     interpolation_port,
                                     nullptr,
                                     nullptr,
                                     nullptr);

    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    const auto clear_for_formal_path = GetPrivateMember(ClearInterpolationBufferForFormalPathTag{});

    const auto result = (service.*clear_for_formal_path)(
        &stop_flag,
        &pause_flag,
        &pause_applied_flag);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(interpolation_port->clear_calls, 4);
    EXPECT_GE(interpolation_port->status_reads, 8);
}

TEST(DispensingProcessServiceWaitForMotionCompleteTest,
     ClearInterpolationBufferForFormalPathTimesOutWhenSettledClearNeverSucceeds) {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    interpolation_port->status_sequence = {
        CoordinateSystemStatus{CoordinateSystemState::IDLE, false, 0U, 0.0f, 0x11, 1, 0}};
    interpolation_port->scripted_clear_failures = {true};
    interpolation_port->scripted_clear_failure_message = "ClearInterpolationBuffer: simulated persistent failure";
    DispensingProcessService service(nullptr,
                                     interpolation_port,
                                     nullptr,
                                     nullptr,
                                     nullptr);

    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    const auto clear_for_formal_path = GetPrivateMember(ClearInterpolationBufferForFormalPathTag{});

    const auto result = (service.*clear_for_formal_path)(
        &stop_flag,
        &pause_flag,
        &pause_applied_flag);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::MOTION_TIMEOUT);
    EXPECT_EQ(result.GetError().GetMessage(), "预定位完成后清缓冲未在超时内成功");
    EXPECT_GT(interpolation_port->clear_calls, 10);
}

TEST(DispensingProcessServiceWaitForMotionCompleteTest, StopExecutionWaitsForCoordinateSystemToDrain) {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    interpolation_port->status_sequence = {
        CoordinateSystemStatus{CoordinateSystemState::MOVING, true, 3U, 12.0f, 0, 1, 3},
        CoordinateSystemStatus{CoordinateSystemState::MOVING, true, 2U, 6.0f, 0, 1, 2},
        CoordinateSystemStatus{CoordinateSystemState::IDLE, false, 0U, 0.0f, 0x10, 1, 0},
        CoordinateSystemStatus{CoordinateSystemState::IDLE, false, 0U, 0.0f, 0x10, 1, 0},
        CoordinateSystemStatus{CoordinateSystemState::IDLE, false, 0U, 0.0f, 0x10, 1, 0},
        CoordinateSystemStatus{CoordinateSystemState::IDLE, false, 0U, 0.0f, 0x10, 1, 0},
        CoordinateSystemStatus{CoordinateSystemState::IDLE, false, 0U, 0.0f, 0x10, 1, 0},
        CoordinateSystemStatus{CoordinateSystemState::IDLE, false, 0U, 0.0f, 0x10, 1, 0}};
    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(std::vector<Point2D>{Point2D{0.0f, 0.0f}});
    DispensingProcessService service(nullptr,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);
    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{true};

    const auto start = std::chrono::steady_clock::now();
    service.StopExecution(&stop_flag, &pause_flag);
    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

    EXPECT_TRUE(stop_flag.load());
    EXPECT_FALSE(pause_flag.load());
    EXPECT_EQ(interpolation_port->stop_calls, 1);
    EXPECT_GE(interpolation_port->status_reads, 6);
    EXPECT_GE(elapsed_ms, 200);
}

}  // namespace
