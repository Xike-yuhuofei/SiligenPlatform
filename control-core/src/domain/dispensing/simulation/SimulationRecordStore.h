#pragma once

#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <cstddef>
#include <vector>

namespace Siligen::Domain::Dispensing::Simulation {

using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::uint64;

/**
 * @brief A single sample captured from the simulation/dispensing loop.
 */
struct SimulationRecord {
    uint64 sequence_id = 0;
    uint64 timestamp_ms = 0;
    float32 target_volume_ul = 0.0f;
    float32 measured_volume_ul = 0.0f;
    float32 position_mm = 0.0f;
};

/**
 * @brief Record sink interface for closed-loop data capture.
 */
class ISimulationRecordStore {
   public:
    virtual ~ISimulationRecordStore() = default;

    virtual Result<void> Append(const SimulationRecord& record) noexcept = 0;

    /**
     * @brief List records in insertion order.
     * @param max_count 0 means return all, otherwise return the most recent N records.
     */
    virtual Result<std::vector<SimulationRecord>> List(size_t max_count = 0) const noexcept = 0;

    virtual Result<void> Clear() noexcept = 0;
    virtual size_t Size() const noexcept = 0;
};

/**
 * @brief Minimal in-memory implementation for unit tests and offline usage.
 */
class InMemorySimulationRecordStore final : public ISimulationRecordStore {
   public:
    Result<void> Append(const SimulationRecord& record) noexcept override {
        records_.push_back(record);
        return Result<void>::Success();
    }

    Result<std::vector<SimulationRecord>> List(size_t max_count = 0) const noexcept override {
        if (max_count == 0 || max_count >= records_.size()) {
            return Result<std::vector<SimulationRecord>>::Success(records_);
        }
        const size_t start = records_.size() - max_count;
        std::vector<SimulationRecord> slice;
        slice.reserve(max_count);
        for (size_t i = start; i < records_.size(); ++i) {
            slice.push_back(records_[i]);
        }
        return Result<std::vector<SimulationRecord>>::Success(std::move(slice));
    }

    Result<void> Clear() noexcept override {
        records_.clear();
        return Result<void>::Success();
    }

    size_t Size() const noexcept override { return records_.size(); }

   private:
    std::vector<SimulationRecord> records_;
};

}  // namespace Siligen::Domain::Dispensing::Simulation
