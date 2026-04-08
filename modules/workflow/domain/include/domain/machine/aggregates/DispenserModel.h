#pragma once

// Bridge-only public surface. Canonical machine aggregate owner now lives in M5 coordinate-alignment.
#include "../../../../../../coordinate-alignment/domain/machine/aggregates/DispenserModel.h"

namespace Siligen::Domain::Machine::Aggregates {

using DispenserModel = Legacy::DispenserModel;
using DispensingTask = Legacy::DispensingTask;

}  // namespace Siligen::Domain::Machine::Aggregates

