#include "siligen/device/adapters/drivers/multicard/MockMultiCard.h"
#include "siligen/device/adapters/drivers/multicard/MockMultiCardWrapper.h"
#include "siligen/device/adapters/motion/InterpolationAdapter.h"

#include <gtest/gtest.h>

#include <chrono>
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
                     double synAccMax) noexcept override {
        configured_ = true;
        return MockMultiCardWrapper::MC_SetCrdPrm(nCrdNum, dimension, profile, synVelMax, synAccMax);
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

}  // namespace
