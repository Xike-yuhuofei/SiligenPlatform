#include "process_planning/contracts/ConfigurationContracts.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace {

using Siligen::Domain::Configuration::Ports::DispensingConfig;
using Siligen::Domain::Configuration::Ports::DxfImportConfig;
using Siligen::Domain::Configuration::Ports::DxfTrajectoryConfig;
using Siligen::Domain::Configuration::Ports::HomingConfig;
using Siligen::Domain::Configuration::Ports::MachineConfig;
using Siligen::Shared::Types::DispensingStrategy;

std::filesystem::path GoldenPath() {
    return std::filesystem::path(__FILE__).parent_path() / "configuration.defaults.txt";
}

std::string ReadTextFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    EXPECT_TRUE(in.is_open()) << "missing golden file: " << path.string();
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

const char* DispensingModeName(Siligen::DispensingMode mode) {
    switch (mode) {
        case Siligen::DispensingMode::CONTACT:
            return "CONTACT";
        case Siligen::DispensingMode::NON_CONTACT:
            return "NON_CONTACT";
        case Siligen::DispensingMode::JETTING:
            return "JETTING";
        case Siligen::DispensingMode::POSITION_TRIGGER:
            return "POSITION_TRIGGER";
    }
    return "UNKNOWN";
}

const char* DispensingStrategyName(DispensingStrategy strategy) {
    switch (strategy) {
        case DispensingStrategy::BASELINE:
            return "BASELINE";
        case DispensingStrategy::SEGMENTED:
            return "SEGMENTED";
        case DispensingStrategy::SUBSEGMENTED:
            return "SUBSEGMENTED";
        case DispensingStrategy::CRUISE_ONLY:
            return "CRUISE_ONLY";
    }
    return "UNKNOWN";
}

std::string SerializeDefaults() {
    const DispensingConfig dispensing;
    const DxfImportConfig preprocess;
    const DxfTrajectoryConfig trajectory;
    const MachineConfig machine;
    const HomingConfig homing;

    std::ostringstream out;
    out << std::boolalpha << std::fixed << std::setprecision(6);
    out << "dispensing.mode=" << DispensingModeName(dispensing.mode) << "\n";
    out << "dispensing.strategy=" << DispensingStrategyName(dispensing.strategy) << "\n";
    out << "dispensing.subsegment_count=" << dispensing.subsegment_count << "\n";
    out << "dispensing.pressure=" << dispensing.pressure << "\n";
    out << "dispensing.wait_time=" << dispensing.wait_time << "\n";
    out << "dispensing.supply_stabilization_ms=" << dispensing.supply_stabilization_ms << "\n";
    out << "dispensing.retract_enabled=" << dispensing.retract_enabled << "\n";
    out << "dispensing.trajectory_sample_dt=" << dispensing.trajectory_sample_dt << "\n";
    out << "dxf_import.normalize_units=" << preprocess.normalize_units << "\n";
    out << "dxf_import.chordal=" << preprocess.chordal << "\n";
    out << "dxf_trajectory.python=" << trajectory.python << "\n";
    out << "dxf_trajectory.script=" << trajectory.script << "\n";
    out << "machine.work_area_width=" << machine.work_area_width << "\n";
    out << "machine.work_area_height=" << machine.work_area_height << "\n";
    out << "machine.positioning_tolerance=" << machine.positioning_tolerance << "\n";
    out << "machine.pulse_per_mm=" << machine.pulse_per_mm << "\n";
    out << "machine.soft_limits.x_max=" << machine.soft_limits.x_max << "\n";
    out << "machine.soft_limits.z_min=" << machine.soft_limits.z_min << "\n";
    out << "homing.mode=" << homing.mode << "\n";
    out << "homing.direction=" << homing.direction << "\n";
    out << "homing.search_distance=" << homing.search_distance << "\n";
    out << "homing.escape_distance=" << homing.escape_distance << "\n";
    out << "homing.timeout_ms=" << homing.timeout_ms << "\n";
    out << "homing.home_backoff_enabled=" << homing.home_backoff_enabled << "\n";
    out << "homing.enable_limit_switch=" << homing.enable_limit_switch << "\n";
    return out.str();
}

TEST(ConfigurationDefaultsGoldenTest, FrozenDefaultsMatchGoldenSnapshot) {
    const auto actual = SerializeDefaults();
    const auto expected = ReadTextFile(GoldenPath());

    EXPECT_EQ(actual, expected);
}

}  // namespace
