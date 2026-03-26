#include <gtest/gtest.h>

#include "siligen/shared/axis_types.h"
#include "siligen/shared/result.h"

namespace {

using Siligen::SharedKernel::AxisName;
using Siligen::SharedKernel::Error;
using Siligen::SharedKernel::ErrorCode;
using Siligen::SharedKernel::FromUserInput;
using Siligen::SharedKernel::LogicalAxisId;
using Siligen::SharedKernel::Result;
using Siligen::SharedKernel::SdkAxisId;
using Siligen::SharedKernel::ToLogicalAxis;
using Siligen::SharedKernel::ToSdkAxis;
using Siligen::SharedKernel::ToUserDisplay;

TEST(SharedKernelResultTest, MapPropagatesSuccessAndError) {
    auto success_input = Result<int>::Success(4);
    auto success = success_input.Map([](int value) { return value * 3; });
    ASSERT_TRUE(success.IsSuccess());
    EXPECT_EQ(success.Value(), 12);

    auto failure = Result<int>::Failure(Error(ErrorCode::INVALID_PARAMETER, "bad input", "shared-kernel"));
    auto mapped_failure = failure.Map([](int value) { return value * 3; });
    ASSERT_TRUE(mapped_failure.IsError());
    EXPECT_EQ(mapped_failure.GetError().GetCode(), ErrorCode::INVALID_PARAMETER);
    EXPECT_EQ(mapped_failure.GetError().GetModule(), "shared-kernel");
}

TEST(SharedKernelResultTest, VoidFlatMapShortCircuitsOnError) {
    auto success = Result<void>::Success().FlatMap([] {
        return Result<int>::Success(7);
    });
    ASSERT_TRUE(success.IsSuccess());
    EXPECT_EQ(success.Value(), 7);

    auto failure = Result<void>::Failure(Error(ErrorCode::NOT_IMPLEMENTED, "disabled", "shared-kernel"));
    auto chained_failure = failure.FlatMap([] {
        return Result<int>::Success(11);
    });
    ASSERT_TRUE(chained_failure.IsError());
    EXPECT_EQ(chained_failure.GetError().GetCode(), ErrorCode::NOT_IMPLEMENTED);
}

TEST(SharedKernelAxisTypesTest, AxisConversionsStayStable) {
    EXPECT_EQ(ToSdkAxis(LogicalAxisId::X), SdkAxisId::X);
    EXPECT_EQ(ToSdkAxis(LogicalAxisId::U), SdkAxisId::U);
    EXPECT_EQ(ToLogicalAxis(SdkAxisId::Z), LogicalAxisId::Z);
    EXPECT_EQ(FromUserInput(2), LogicalAxisId::Y);
    EXPECT_EQ(ToUserDisplay(LogicalAxisId::U), 4);
    EXPECT_STREQ(AxisName(LogicalAxisId::INVALID), "INVALID");
}

}  // namespace
