#pragma once

#include <cstddef>
#include <vector>

#include "sim/control/io_model.h"
#include "sim/simulation_context.h"

namespace sim {

class TriggerModel {
public:
    void initialize(std::vector<TriggerSpec> triggers);
    void update(double axis_position, double now, IOModel& io_model);

private:
    std::vector<TriggerSpec> triggers_{};
    std::size_t next_trigger_index_{0};
};

}  // namespace sim
