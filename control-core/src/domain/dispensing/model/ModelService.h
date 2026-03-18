#pragma once

#include "domain/dispensing/simulation/SimulationRecordStore.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <vector>

namespace Siligen::Domain::Dispensing::Model {

using Siligen::Domain::Dispensing::Simulation::SimulationRecord;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::float64;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::uint64;

/**
 * @brief Snapshot of model parameters used for compensation decisions.
 */
struct ModelSnapshot {
    uint64 version = 0;
    float32 gain = 1.0f;
    float32 bias = 0.0f;
    float32 last_error = 0.0f;
    bool valid = false;
};

/**
 * @brief Model service interface for closed-loop estimation.
 */
class IModelService {
   public:
    virtual ~IModelService() = default;

    virtual Result<void> UpdateFromRecords(const std::vector<SimulationRecord>& records) noexcept = 0;
    virtual Result<ModelSnapshot> GetSnapshot() const noexcept = 0;
};

/**
 * @brief Minimal model implementation: average error as bias.
 */
class SimpleModelService final : public IModelService {
   public:
    Result<void> UpdateFromRecords(const std::vector<SimulationRecord>& records) noexcept override {
        if (records.empty()) {
            snapshot_.valid = false;
            snapshot_.gain = 1.0f;
            snapshot_.bias = 0.0f;
            snapshot_.last_error = 0.0f;
            return Result<void>::Success();
        }

        float64 sum_error = 0.0;
        for (const auto& record : records) {
            const float32 error = record.measured_volume_ul - record.target_volume_ul;
            sum_error += error;
        }

        snapshot_.bias = static_cast<float32>(sum_error / static_cast<float64>(records.size()));
        snapshot_.gain = 1.0f;
        snapshot_.last_error = records.back().measured_volume_ul - records.back().target_volume_ul;
        snapshot_.valid = true;
        ++snapshot_.version;

        return Result<void>::Success();
    }

    Result<ModelSnapshot> GetSnapshot() const noexcept override {
        return Result<ModelSnapshot>::Success(snapshot_);
    }

   private:
    ModelSnapshot snapshot_{};
};

}  // namespace Siligen::Domain::Dispensing::Model
