#include "coordinate_alignment/contracts/CoordinateTransformSet.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

using Siligen::CoordinateAlignment::Contracts::CoordinateTransformSet;

std::filesystem::path GoldenPath() {
    return std::filesystem::path(__FILE__).parent_path() / "coordinate_transform_set.baseline.txt";
}

std::string ReadTextFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    EXPECT_TRUE(in.is_open()) << "missing golden file: " << path.string();
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

CoordinateTransformSet BuildBaselineSample() {
    CoordinateTransformSet transform_set;
    transform_set.alignment_id = "baseline-alignment";
    transform_set.plan_id = "plan-origin-rotation";
    transform_set.valid = true;
    transform_set.transforms = {
        {"origin-offset", "machine-origin", "alignment-origin"},
        {"rotation-z", "alignment-origin", "process-frame"},
    };
    return transform_set;
}

std::string Serialize(const CoordinateTransformSet& transform_set) {
    std::ostringstream out;
    out << std::boolalpha;
    out << "alignment_id=" << transform_set.alignment_id << "\n";
    out << "plan_id=" << transform_set.plan_id << "\n";
    out << "valid=" << transform_set.valid << "\n";
    out << "owner_module=" << transform_set.owner_module << "\n";
    out << "transform_count=" << transform_set.transforms.size() << "\n";
    for (size_t i = 0; i < transform_set.transforms.size(); ++i) {
        out << "transform[" << i << "].id=" << transform_set.transforms[i].transform_id << "\n";
        out << "transform[" << i << "].source=" << transform_set.transforms[i].source_frame << "\n";
        out << "transform[" << i << "].target=" << transform_set.transforms[i].target_frame << "\n";
    }
    return out.str();
}

TEST(CoordinateTransformSetGoldenTest, BaselineSampleMatchesGoldenSnapshot) {
    const auto actual = Serialize(BuildBaselineSample());
    const auto expected = ReadTextFile(GoldenPath());

    EXPECT_EQ(actual, expected);
}

}  // namespace
