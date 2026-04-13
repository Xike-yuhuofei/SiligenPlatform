#pragma once

#include <cstdint>

namespace Siligen::Workflow::Domain {

enum class RollbackTarget : std::uint8_t {
    ToSourceAccepted = 0,
    ToGeometryReady,
    ToTopologyReady,
    ToFeaturesReady,
    ToProcessPlanned,
    ToCoordinatesResolved,
    ToAligned,
    ToProcessPathReady,
    ToMotionPlanned,
    ToTimingPlanned,
    ToPackageBuilt,
    ToPackageValidated,
};

}  // namespace Siligen::Workflow::Domain
