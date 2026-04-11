#include "application/services/process_path/ProcessPathFacade.h"
#include "support/ProcessPathTestSupport.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {

using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
using Siligen::ProcessPath::Tests::Support::MakeLeadInLeadOutRequest;
using Siligen::ProcessPath::Tests::Support::SerializeBuildResultSummary;

std::filesystem::path GoldenPath() {
    return std::filesystem::path(__FILE__).parent_path() / "lead_on_policy.summary.txt";
}

std::string ReadGoldenFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    EXPECT_TRUE(in.is_open()) << "missing golden file: " << path.string();
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

TEST(LeadOnPolicyGoldenTest, ApproachDefaultsOffAndPrimingMustBeExplicit) {
    ProcessPathFacade facade;
    const auto default_result = facade.Build(MakeLeadInLeadOutRequest(false));
    const auto priming_result = facade.Build(MakeLeadInLeadOutRequest(true));

    std::string actual;
    actual += "[default]\n";
    actual += SerializeBuildResultSummary(default_result);
    actual += "[priming]\n";
    actual += SerializeBuildResultSummary(priming_result);

    const auto expected = ReadGoldenFile(GoldenPath());
    EXPECT_EQ(actual, expected);
}

}  // namespace
