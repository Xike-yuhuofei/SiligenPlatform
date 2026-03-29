#pragma once

#include "../../../../domain/trajectory/value-objects/Primitive.h"

namespace Siligen::ProcessPath::Contracts {

using LinePrimitive = Siligen::Domain::Trajectory::ValueObjects::LinePrimitive;
using ArcPrimitive = Siligen::Domain::Trajectory::ValueObjects::ArcPrimitive;
using SplinePrimitive = Siligen::Domain::Trajectory::ValueObjects::SplinePrimitive;
using CirclePrimitive = Siligen::Domain::Trajectory::ValueObjects::CirclePrimitive;
using EllipsePrimitive = Siligen::Domain::Trajectory::ValueObjects::EllipsePrimitive;
using PointPrimitive = Siligen::Domain::Trajectory::ValueObjects::PointPrimitive;
using ContourElementType = Siligen::Domain::Trajectory::ValueObjects::ContourElementType;
using ContourElement = Siligen::Domain::Trajectory::ValueObjects::ContourElement;
using ContourPrimitive = Siligen::Domain::Trajectory::ValueObjects::ContourPrimitive;
using PrimitiveType = Siligen::Domain::Trajectory::ValueObjects::PrimitiveType;
using Primitive = Siligen::Domain::Trajectory::ValueObjects::Primitive;

}  // namespace Siligen::ProcessPath::Contracts
