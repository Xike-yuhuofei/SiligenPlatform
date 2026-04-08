#pragma once

#include "domain/recovery/redundancy/RedundancyTypes.h"
#include "shared/types/Result.h"

#include <string>
#include <vector>

namespace Siligen::Application::Services::Redundancy {

class CandidateScoringService {
   public:
    Siligen::Shared::Types::Result<std::vector<Siligen::Domain::Recovery::Redundancy::CandidateRecord>> ComputeCandidates(
        const std::vector<Siligen::Domain::Recovery::Redundancy::CodeEntity>& entities,
        const std::vector<Siligen::Domain::Recovery::Redundancy::StaticEvidenceRecord>& static_evidence,
        const std::vector<Siligen::Domain::Recovery::Redundancy::RuntimeEvidenceRecord>& runtime_evidence,
        const std::string& snapshot_id,
        const std::string& policy_version,
        const std::string& computed_at) const;

   private:
    static std::string MakeCandidateId(const std::string& entity_id,
                                       const std::string& module,
                                       const std::string& snapshot_id,
                                       const std::string& policy_version);
};

}  // namespace Siligen::Application::Services::Redundancy
