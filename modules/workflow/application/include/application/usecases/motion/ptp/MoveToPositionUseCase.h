#pragma once

// Transitional public wrapper for legacy workflow/runtime consumers.
// The implementation still lives under workflow/application/usecases during
// the owner-surface migration; runtime-host must consume it through the
// canonical workflow application public include root.
#include "../../../../../usecases/motion/ptp/MoveToPositionUseCase.h"
