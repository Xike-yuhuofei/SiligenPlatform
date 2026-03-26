#pragma once

#include "domain/dispensing/model/ModelService.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

namespace Siligen::Domain::Dispensing::Compensation {

using Siligen::Domain::Dispensing::Model::ModelSnapshot;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::Result;

/**
 * @brief Context input for compensation trigger evaluation.
 */
struct TriggerContext {
    float32 nominal_interval_mm = 0.0f;
    float32 nominal_pulse_width_ms = 0.0f;
};

/**
 * @brief Output of a trigger compensation decision.
 */
struct TriggerCompensation {
    bool enabled = false;
    float32 interval_scale = 1.0f;
    float32 pulse_width_scale = 1.0f;
    float32 open_comp_delta_ms = 0.0f;
    float32 close_comp_delta_ms = 0.0f;
};

/**
 * @brief Trigger strategy interface for model-driven adjustments.
 */
class ITriggerStrategy {
   public:
    virtual ~ITriggerStrategy() = default;

    virtual Result<TriggerCompensation> Evaluate(const ModelSnapshot& model,
                                                 const TriggerContext& context) const noexcept = 0;
};

/**
 * @brief Fixed strategy returning a pre-configured compensation payload.
 */
class FixedTriggerStrategy final : public ITriggerStrategy {
   public:
    explicit FixedTriggerStrategy(const TriggerCompensation& compensation)
        : compensation_(compensation) {}

    Result<TriggerCompensation> Evaluate(const ModelSnapshot& model,
                                         const TriggerContext& context) const noexcept override {
        (void)model;
        (void)context;
        return Result<TriggerCompensation>::Success(compensation_);
    }

   private:
    TriggerCompensation compensation_{};
};

}  // namespace Siligen::Domain::Dispensing::Compensation
