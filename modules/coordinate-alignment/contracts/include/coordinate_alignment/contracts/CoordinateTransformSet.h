#pragma once

#include <string>
#include <vector>

namespace Siligen::CoordinateAlignment::Contracts {

struct CoordinateTransform {
    std::string transform_id;
    std::string source_frame;
    std::string target_frame;
};

struct CoordinateTransformSet {
    std::string alignment_id;
    std::string plan_id;
    bool valid = false;
    std::vector<CoordinateTransform> transforms;
    std::string owner_module = "M5";
};

}  // namespace Siligen::CoordinateAlignment::Contracts
