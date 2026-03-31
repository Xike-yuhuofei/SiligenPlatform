#include "siligen/device/adapters/drivers/multicard/MockMultiCard.h"
#include "siligen/device/adapters/drivers/multicard/MockMultiCardWrapper.h"
#include "siligen/device/adapters/motion/InterpolationAdapter.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace {

using InterpolationAdapter = Siligen::Infrastructure::Adapters::InterpolationAdapter;
using MockMultiCard = Siligen::Infrastructure::Hardware::MockMultiCard;
using MockMultiCardWrapper = Siligen::Infrastructure::Hardware::MockMultiCardWrapper;
using CoordinateSystemConfig = Siligen::Domain::Motion::Ports::CoordinateSystemConfig;
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

}  // namespace
