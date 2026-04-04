#pragma once

#include "process_path/contracts/Primitive.h"

// Legacy bridge header; canonical public owner contract lives in M6 process-path contracts.
namespace Siligen::Domain::Trajectory::ValueObjects {

using LinePrimitive = Siligen::ProcessPath::Contracts::LinePrimitive;
using ArcPrimitive = Siligen::ProcessPath::Contracts::ArcPrimitive;
using SplinePrimitive = Siligen::ProcessPath::Contracts::SplinePrimitive;
using CirclePrimitive = Siligen::ProcessPath::Contracts::CirclePrimitive;
using EllipsePrimitive = Siligen::ProcessPath::Contracts::EllipsePrimitive;
using PointPrimitive = Siligen::ProcessPath::Contracts::PointPrimitive;
using ContourElementType = Siligen::ProcessPath::Contracts::ContourElementType;
using ContourElement = Siligen::ProcessPath::Contracts::ContourElement;
using ContourPrimitive = Siligen::ProcessPath::Contracts::ContourPrimitive;
using PrimitiveType = Siligen::ProcessPath::Contracts::PrimitiveType;
using Primitive = Siligen::ProcessPath::Contracts::Primitive;

}  // namespace Siligen::Domain::Trajectory::ValueObjects
