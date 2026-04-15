#include "siligen/device/adapters/drivers/multicard/MockMultiCard.h"
#include "siligen/device/adapters/drivers/multicard/MockMultiCardWrapper.h"
#include "siligen/device/adapters/motion/InterpolationAdapter.h"
#include "siligen/device/adapters/motion/MultiCardMotionAdapter.h"

#include <gtest/gtest.h>

#include <chrono>
#include <array>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

using InterpolationAdapter = Siligen::Infrastructure::Adapters::InterpolationAdapter;
using MockMultiCard = Siligen::Infrastructure::Hardware::MockMultiCard;
using MockMultiCardWrapper = Siligen::Infrastructure::Hardware::MockMultiCardWrapper;
using CoordinateSystemConfig = Siligen::Domain::Motion::Ports::CoordinateSystemConfig;
using InterpolationData = Siligen::RuntimeExecution::Contracts::Motion::InterpolationData;
using InterpolationType = Siligen::RuntimeExecution::Contracts::Motion::InterpolationType;
using LogicalAxisId = Siligen::Shared::Types::LogicalAxisId;
using HardwareConfiguration = Siligen::Shared::Types::HardwareConfiguration;
using MultiCardMotionAdapter = Siligen::Infrastructure::Adapters::MultiCardMotionAdapter;

class FlakyPostConfigureClearWrapper final : public MockMultiCardWrapper {
   public:
    FlakyPostConfigureClearWrapper(int retriable_failures, int terminal_error = 0)
        : MockMultiCardWrapper(std::make_shared<MockMultiCard>()),
          retriable_failures_remaining_(retriable_failures),
          terminal_error_(terminal_error) {}

    int MC_SetCrdPrm(short nCrdNum,
                     short dimension,
                     short* profile,
                     double synVelMax,
                     double synAccMax,
                     short setOriginFlag = 0,
                     const long* originPos = nullptr) noexcept override {
        configured_ = true;
        return MockMultiCardWrapper::MC_SetCrdPrm(
            nCrdNum,
            dimension,
            profile,
            synVelMax,
            synAccMax,
            setOriginFlag,
            originPos);
    }

    int MC_CrdClear(short crd, short fifo) noexcept override {
        ++crd_clear_calls_;
        if (configured_) {
            if (retriable_failures_remaining_ > 0) {
                --retriable_failures_remaining_;
                return 1;
            }
            if (terminal_error_ != 0) {
                return terminal_error_;
            }
        }
        return MockMultiCardWrapper::MC_CrdClear(crd, fifo);
    }

    int MC_StopEx(long mask, short stopMode) noexcept override {
        ++stop_ex_calls_;
        return MockMultiCardWrapper::MC_StopEx(mask, stopMode);
    }

    int crd_clear_calls() const noexcept { return crd_clear_calls_; }
    int stop_ex_calls() const noexcept { return stop_ex_calls_; }

   private:
    int retriable_failures_remaining_ = 0;
    int terminal_error_ = 0;
    bool configured_ = false;
    int crd_clear_calls_ = 0;
    int stop_ex_calls_ = 0;
};

class FlakyDirectClearWrapper final : public MockMultiCardWrapper {
   public:
    FlakyDirectClearWrapper(int retriable_failures, int terminal_error = 0)
        : MockMultiCardWrapper(std::make_shared<MockMultiCard>()),
          retriable_failures_remaining_(retriable_failures),
          terminal_error_(terminal_error) {}

    int MC_CrdClear(short crd, short fifo) noexcept override {
        ++crd_clear_calls_;
        if (retriable_failures_remaining_ > 0) {
            --retriable_failures_remaining_;
            return 1;
        }
        if (terminal_error_ != 0) {
            return terminal_error_;
        }
        return MockMultiCardWrapper::MC_CrdClear(crd, fifo);
    }

    int MC_StopEx(long mask, short stopMode) noexcept override {
        ++stop_ex_calls_;
        return MockMultiCardWrapper::MC_StopEx(mask, stopMode);
    }

    int crd_clear_calls() const noexcept { return crd_clear_calls_; }
    int stop_ex_calls() const noexcept { return stop_ex_calls_; }

   private:
    int retriable_failures_remaining_ = 0;
    int terminal_error_ = 0;
    int crd_clear_calls_ = 0;
    int stop_ex_calls_ = 0;
};

class StaleStoppedVelocityWrapper final : public MockMultiCardWrapper {
   public:
    StaleStoppedVelocityWrapper()
        : MockMultiCardWrapper(std::make_shared<MockMultiCard>()) {}

    int MC_CrdStatus(short nCrdNum, short* pCrdStatus, long* pSegment, short FifoIndex) noexcept override {
        (void)nCrdNum;
        (void)FifoIndex;
        if (pCrdStatus != nullptr) {
            *pCrdStatus = 0x0002;
        }
        if (pSegment != nullptr) {
            *pSegment = -858993460L;
        }
        return 0;
    }

    int MC_GetCrdVel(short nCrdNum, double* pSynVel) noexcept override {
        (void)nCrdNum;
        if (pSynVel != nullptr) {
            *pSynVel = 1.0;
        }
        return 0;
    }
};

class RecordingOriginWrapper final : public MockMultiCardWrapper {
   public:
    RecordingOriginWrapper()
        : MockMultiCardWrapper(std::make_shared<MockMultiCard>()) {}

    int MC_SetCrdPrm(short nCrdNum,
                     short dimension,
                     short* profile,
                     double synVelMax,
                     double synAccMax,
                     short setOriginFlag = 0,
                     const long* originPos = nullptr) noexcept override {
        last_set_origin_flag = setOriginFlag;
        last_origin_pos.fill(0);
        if (originPos != nullptr) {
            for (std::size_t i = 0; i < last_origin_pos.size(); ++i) {
                last_origin_pos[i] = originPos[i];
            }
        }
        return MockMultiCardWrapper::MC_SetCrdPrm(
            nCrdNum,
            dimension,
            profile,
            synVelMax,
            synAccMax,
            setOriginFlag,
            originPos);
    }

    short last_set_origin_flag = -1;
    std::array<long, 8> last_origin_pos{};
};

CoordinateSystemConfig BuildConfig() {
    CoordinateSystemConfig config;
    config.dimension = 2;
    config.axis_map = {LogicalAxisId::X, LogicalAxisId::Y};
    config.max_velocity = 50.0f;
    config.max_acceleration = 100.0f;
    return config;
}

InterpolationData BuildLinearMove(float x_mm, float y_mm) {
    InterpolationData line;
    line.type = InterpolationType::LINEAR;
    line.positions = {x_mm, y_mm};
    line.velocity = 30.0f;
    line.acceleration = 80.0f;
    line.end_velocity = 0.0f;
    return line;
}

TEST(InterpolationAdapterTest, ConfigureCoordinateSystemRetriesTransientCrdClearExecutionFailure) {
    auto wrapper = std::make_shared<FlakyPostConfigureClearWrapper>(2);
    InterpolationAdapter adapter(wrapper);

    const auto result = adapter.ConfigureCoordinateSystem(1, BuildConfig());

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(wrapper->crd_clear_calls(), 4);
    EXPECT_EQ(wrapper->stop_ex_calls(), 3);
}

TEST(InterpolationAdapterTest, ConfigureCoordinateSystemDoesNotMaskNonRetriableCrdClearFailure) {
    auto wrapper = std::make_shared<FlakyPostConfigureClearWrapper>(2, 7);
    InterpolationAdapter adapter(wrapper);

    const auto result = adapter.ConfigureCoordinateSystem(1, BuildConfig());

    ASSERT_TRUE(result.IsError());
    EXPECT_NE(result.GetError().GetMessage().find("ConfigureCoordinateSystem: CrdClear failed"), std::string::npos);
    EXPECT_NE(result.GetError().GetMessage().find("错误码:7"), std::string::npos);
    EXPECT_EQ(wrapper->crd_clear_calls(), 4);
}

TEST(InterpolationAdapterTest, ConfigureCoordinateSystemDefaultsToCurrentPlannedPositionOrigin) {
    auto wrapper = std::make_shared<RecordingOriginWrapper>();
    InterpolationAdapter adapter(wrapper);

    const auto result = adapter.ConfigureCoordinateSystem(1, BuildConfig());

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(wrapper->last_set_origin_flag, 0);
    for (long value : wrapper->last_origin_pos) {
        EXPECT_EQ(value, 0);
    }
}

TEST(InterpolationAdapterTest, ConfigureCoordinateSystemCanPinOriginToMachineZero) {
    auto wrapper = std::make_shared<RecordingOriginWrapper>();
    InterpolationAdapter adapter(wrapper);
    auto config = BuildConfig();
    config.use_current_planned_position_as_origin = false;

    const auto result = adapter.ConfigureCoordinateSystem(1, config);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(wrapper->last_set_origin_flag, 1);
    for (long value : wrapper->last_origin_pos) {
        EXPECT_EQ(value, 0);
    }
}

TEST(InterpolationAdapterTest, ClearInterpolationBufferRetriesTransientCrdClearExecutionFailure) {
    auto wrapper = std::make_shared<FlakyDirectClearWrapper>(2);
    InterpolationAdapter adapter(wrapper);

    const auto result = adapter.ClearInterpolationBuffer(1);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(wrapper->crd_clear_calls(), 3);
    EXPECT_EQ(wrapper->stop_ex_calls(), 2);
}

TEST(InterpolationAdapterTest, ClearInterpolationBufferDoesNotMaskNonRetriableCrdClearFailure) {
    auto wrapper = std::make_shared<FlakyDirectClearWrapper>(0, 7);
    InterpolationAdapter adapter(wrapper);

    const auto result = adapter.ClearInterpolationBuffer(1);

    ASSERT_TRUE(result.IsError());
    EXPECT_NE(result.GetError().GetMessage().find("ClearInterpolationBuffer: CrdClear failed"), std::string::npos);
    EXPECT_NE(result.GetError().GetMessage().find("错误码:7"), std::string::npos);
    EXPECT_EQ(wrapper->crd_clear_calls(), 1);
    EXPECT_EQ(wrapper->stop_ex_calls(), 0);
}

TEST(InterpolationAdapterTest, AddInterpolationDataUsesCenterOffsetRelativeToBufferedArcStart) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);
    InterpolationAdapter adapter(wrapper);

    const auto configure_result = adapter.ConfigureCoordinateSystem(1, BuildConfig());
    ASSERT_TRUE(configure_result.IsSuccess()) << configure_result.GetError().GetMessage();

    InterpolationData line;
    line.type = InterpolationType::LINEAR;
    line.positions = {20.0f, 5.0f};
    line.velocity = 30.0f;
    line.acceleration = 80.0f;
    line.end_velocity = 12.0f;
    const auto line_result = adapter.AddInterpolationData(1, line);
    ASSERT_TRUE(line_result.IsSuccess()) << line_result.GetError().GetMessage();

    InterpolationData arc;
    arc.type = InterpolationType::CIRCULAR_CCW;
    arc.positions = {30.0f, 15.0f};
    arc.velocity = 25.0f;
    arc.acceleration = 80.0f;
    arc.end_velocity = 0.0f;
    arc.center_x = 25.0f;
    arc.center_y = 5.0f;
    const auto arc_result = adapter.AddInterpolationData(1, arc);
    ASSERT_TRUE(arc_result.IsSuccess()) << arc_result.GetError().GetMessage();

    const auto buffered_arcs = mock_card->GetBufferedArcSegments(1);
    ASSERT_EQ(buffered_arcs.size(), 1U);
    EXPECT_EQ(buffered_arcs.front().center_x, 1000);
    EXPECT_EQ(buffered_arcs.front().center_y, 0);
    EXPECT_EQ(buffered_arcs.front().direction, 0);
}

TEST(InterpolationAdapterTest, GetCoordinateSystemStatusReportsMovingImmediatelyAfterStartInMockMode) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);
    InterpolationAdapter adapter(wrapper);

    const auto configure_result = adapter.ConfigureCoordinateSystem(1, BuildConfig());
    ASSERT_TRUE(configure_result.IsSuccess()) << configure_result.GetError().GetMessage();

    const auto add_result = adapter.AddInterpolationData(1, BuildLinearMove(1.0f, 0.0f));
    ASSERT_TRUE(add_result.IsSuccess()) << add_result.GetError().GetMessage();

    const auto start_result = adapter.StartCoordinateSystemMotion(0x01);
    ASSERT_TRUE(start_result.IsSuccess()) << start_result.GetError().GetMessage();

    const auto status_result = adapter.GetCoordinateSystemStatus(1);
    ASSERT_TRUE(status_result.IsSuccess()) << status_result.GetError().GetMessage();
    EXPECT_TRUE(status_result.Value().is_moving);
    EXPECT_GT(status_result.Value().remaining_segments, 0U);
    EXPECT_GT(status_result.Value().current_velocity, 0.0f);
}

TEST(InterpolationAdapterTest, GetCoordinateSystemStatusClearsStaleVelocityWhenControllerAlreadyStopped) {
    auto wrapper = std::make_shared<StaleStoppedVelocityWrapper>();
    InterpolationAdapter adapter(wrapper);

    const auto status_result = adapter.GetCoordinateSystemStatus(1);

    ASSERT_TRUE(status_result.IsSuccess()) << status_result.GetError().GetMessage();
    EXPECT_FALSE(status_result.Value().is_moving);
    EXPECT_EQ(status_result.Value().state, Siligen::Domain::Motion::Ports::CoordinateSystemState::IDLE);
    EXPECT_EQ(status_result.Value().remaining_segments, 0U);
    EXPECT_EQ(status_result.Value().raw_status_word, 0x0002);
    EXPECT_EQ(status_result.Value().raw_segment, 0);
    EXPECT_FLOAT_EQ(status_result.Value().current_velocity, 0.0f);
}

TEST(InterpolationAdapterTest, MockCoordinateMotionAdvancesOnReadWithoutExplicitTick) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);
    InterpolationAdapter adapter(wrapper);

    const auto configure_result = adapter.ConfigureCoordinateSystem(1, BuildConfig());
    ASSERT_TRUE(configure_result.IsSuccess()) << configure_result.GetError().GetMessage();

    const auto add_result = adapter.AddInterpolationData(1, BuildLinearMove(5.0f, 0.0f));
    ASSERT_TRUE(add_result.IsSuccess()) << add_result.GetError().GetMessage();

    const auto start_result = adapter.StartCoordinateSystemMotion(0x01);
    ASSERT_TRUE(start_result.IsSuccess()) << start_result.GetError().GetMessage();

    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    long partial_x = 0;
    ASSERT_EQ(wrapper->MC_GetPos(1, &partial_x), 0);
    EXPECT_GT(partial_x, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(130));

    const auto status_result = adapter.GetCoordinateSystemStatus(1);
    ASSERT_TRUE(status_result.IsSuccess()) << status_result.GetError().GetMessage();
    EXPECT_FALSE(status_result.Value().is_moving);
    EXPECT_EQ(status_result.Value().remaining_segments, 0U);
    EXPECT_GT(status_result.Value().raw_status_word & 0x10, 0);

    long final_x = 0;
    ASSERT_EQ(wrapper->MC_GetPos(1, &final_x), 0);
    EXPECT_EQ(final_x, 1000);
}

TEST(InterpolationAdapterTest, StopCoordinateSystemMotionDoesNotStopStandaloneJogAxis) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);
    InterpolationAdapter interpolation_adapter(wrapper);

    HardwareConfiguration hardware_config;
    hardware_config.num_axes = 2;
    hardware_config.pulse_per_mm = 200.0f;
    hardware_config.max_acceleration_mm_s2 = 500.0f;
    hardware_config.max_deceleration_mm_s2 = 500.0f;

    auto motion_adapter_result = MultiCardMotionAdapter::Create(wrapper, hardware_config);
    ASSERT_TRUE(motion_adapter_result.IsSuccess()) << motion_adapter_result.GetError().GetMessage();

    auto jog_result = motion_adapter_result.Value()->StartJog(LogicalAxisId::X, 1, 10.0f);
    ASSERT_TRUE(jog_result.IsSuccess()) << jog_result.GetError().GetMessage();

    auto before_stop_status = motion_adapter_result.Value()->GetAxisStatus(LogicalAxisId::X);
    ASSERT_TRUE(before_stop_status.IsSuccess()) << before_stop_status.GetError().GetMessage();
    EXPECT_EQ(before_stop_status.Value().state, Siligen::Domain::Motion::Ports::MotionState::MOVING);

    const auto stop_result = interpolation_adapter.StopCoordinateSystemMotion(0x01);
    ASSERT_TRUE(stop_result.IsSuccess()) << stop_result.GetError().GetMessage();

    auto after_stop_status = motion_adapter_result.Value()->GetAxisStatus(LogicalAxisId::X);
    ASSERT_TRUE(after_stop_status.IsSuccess()) << after_stop_status.GetError().GetMessage();
    EXPECT_EQ(after_stop_status.Value().state, Siligen::Domain::Motion::Ports::MotionState::MOVING);
    EXPECT_GT(after_stop_status.Value().velocity, 0.0f);
}

}  // namespace
