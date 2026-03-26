#include <gtest/gtest.h>

#include "siligen/shared/strings/string_manipulator.h"

namespace {

using Siligen::SharedKernel::StringManipulator;

TEST(SharedKernelStringManipulatorTest, StripInlineCommentOnlyRemovesWhitespacePrefixedMarkers) {
    EXPECT_EQ(StringManipulator::StripInlineComment(" value ; comment "), "value");
    EXPECT_EQ(StringManipulator::StripInlineComment("value#comment"), "value#comment");
    EXPECT_EQ(StringManipulator::StripInlineComment("value # comment"), "value");
}

TEST(SharedKernelStringManipulatorTest, SplitRespectsMaxSplit) {
    const auto values = StringManipulator::Split("alpha::beta::gamma", "::", 2);
    ASSERT_EQ(values.size(), 2U);
    EXPECT_EQ(values[0], "alpha");
    EXPECT_EQ(values[1], "beta::gamma");
}

TEST(SharedKernelStringManipulatorTest, JoinAndPadRemainDeterministic) {
    const std::vector<std::string> values{"x", "y", "z"};
    const auto joined = StringManipulator::Join(values, "-");
    EXPECT_EQ(joined, "x-y-z");
    EXPECT_EQ(StringManipulator::PadLeft("42", 4, '0'), "0042");
    EXPECT_EQ(StringManipulator::PadRight("42", 4, '0'), "4200");
}

}  // namespace
