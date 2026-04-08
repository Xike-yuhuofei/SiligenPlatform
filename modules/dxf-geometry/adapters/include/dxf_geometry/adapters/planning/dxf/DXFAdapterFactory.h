#pragma once

#include "dxf_geometry/adapters/planning/dxf/AutoPathSourceAdapter.h"
#include "process_path/contracts/IDXFPathSourcePort.h"
#include "shared/types/Result.h"

#include <memory>

namespace Siligen::Infrastructure::Adapters::Parsing {

using Siligen::Domain::Trajectory::Ports::IDXFPathSourcePort;

class DXFAdapterFactory {
   public:
    enum class AdapterType {
        LOCAL,
        REMOTE,
        MOCK
    };

    static Siligen::Shared::Types::Result<std::shared_ptr<IDXFPathSourcePort>> CreateDXFPathSourceAdapter(
        AdapterType type = AdapterType::LOCAL);

    static bool CheckAdapterHealth(const std::shared_ptr<IDXFPathSourcePort>& adapter);
    static AdapterType GetCurrentAdapterType();

   private:
    static AdapterType current_adapter_type_;

    static std::shared_ptr<IDXFPathSourcePort> CreateLocalAdapter();
    static std::shared_ptr<IDXFPathSourcePort> CreateRemoteAdapter();
    static std::shared_ptr<IDXFPathSourcePort> CreateMockAdapter();
};

}  // namespace Siligen::Infrastructure::Adapters::Parsing
