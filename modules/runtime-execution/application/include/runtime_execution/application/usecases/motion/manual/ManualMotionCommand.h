#pragma once

#include "shared/types/Types.h"

namespace Siligen::Application::UseCases::Motion::Manual {

struct ManualMotionCommand {
    Siligen::Shared::Types::LogicalAxisId axis = Siligen::Shared::Types::LogicalAxisId::X;
    float32 position = 0.0f;
    float32 velocity = 0.0f;
    float32 acceleration = 0.0f;
    bool relative_move = false;
};

}  // namespace Siligen::Application::UseCases::Motion::Manual
