#include "infrastructure/adapters/planning/dxf/DXFAdapterFactory.h"
#include "infrastructure/adapters/planning/dxf/PbPathSourceAdapter.h"

#include "shared/types/Error.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

using Siligen::Infrastructure::Adapters::Parsing::DXFAdapterFactory;
using Siligen::Infrastructure::Adapters::Parsing::PbPathSourceAdapter;
using Siligen::Shared::Types::ErrorCode;

int Fail(const std::string& message) {
    std::cerr << message << std::endl;
    return 1;
}

void SetEnvVar(const std::string& name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name.c_str(), value.c_str());
#else
    setenv(name.c_str(), value.c_str(), 1);
#endif
}

void UnsetEnvVar(const std::string& name) {
#ifdef _WIN32
    _putenv_s(name.c_str(), "");
#else
    unsetenv(name.c_str());
#endif
}

}  // namespace

int main() {
    PbPathSourceAdapter protobufAdapter;
    const auto protobufResult = protobufAdapter.LoadFromFile("disabled.pb");

    if (!protobufResult.IsError()) {
        return Fail("expected protobuf ingress smoke to fail explicitly for missing or disabled input");
    }

    const auto& protobufError = protobufResult.GetError();
    const auto protobufMessage = protobufError.GetMessage();
    if (protobufError.GetCode() == ErrorCode::FILE_NOT_FOUND) {
        if (protobufMessage.find("disabled.pb") == std::string::npos) {
            return Fail("expected FILE_NOT_FOUND protobuf message to mention disabled.pb");
        }
    } else if (protobufError.GetCode() == ErrorCode::NOT_IMPLEMENTED) {
        if (protobufMessage.find("SILIGEN_ENABLE_PROTOBUF=OFF") == std::string::npos) {
            return Fail("expected disabled protobuf message to mention SILIGEN_ENABLE_PROTOBUF=OFF");
        }
    } else {
        return Fail("expected FILE_NOT_FOUND or NOT_IMPLEMENTED from protobuf ingress smoke");
    }

    UnsetEnvVar("SILIGEN_DXF_AUTOPATH_LEGACY");
    auto localAdapter = DXFAdapterFactory::CreateDXFPathSourceAdapter(DXFAdapterFactory::AdapterType::LOCAL);
    if (!localAdapter) {
        return Fail("failed to create local DXF adapter");
    }

    const auto localDefaultResult = localAdapter->LoadDXFFile("legacy_input.dxf");
    if (!localDefaultResult.IsError()) {
        return Fail("expected local DXF adapter to reject legacy autopath by default");
    }
    if (localDefaultResult.GetError().GetCode() != ErrorCode::CONFIGURATION_ERROR) {
        return Fail("expected CONFIGURATION_ERROR when legacy DXF autopath is not explicitly enabled");
    }

    SetEnvVar("SILIGEN_DXF_AUTOPATH_LEGACY", "1");
    localAdapter = DXFAdapterFactory::CreateDXFPathSourceAdapter(DXFAdapterFactory::AdapterType::LOCAL);
    if (!localAdapter) {
        UnsetEnvVar("SILIGEN_DXF_AUTOPATH_LEGACY");
        return Fail("failed to recreate local DXF adapter with legacy override enabled");
    }

    const auto localEnabledResult = localAdapter->LoadDXFFile("legacy_input.dxf");
    UnsetEnvVar("SILIGEN_DXF_AUTOPATH_LEGACY");
    if (!localEnabledResult.IsSuccess()) {
        return Fail("expected local DXF adapter override to return a structured result");
    }
    if (localEnabledResult.Value().success) {
        return Fail("expected local DXF adapter override to stay in explicit non-success stub mode");
    }
    if (localEnabledResult.Value().error_message.empty()) {
        return Fail("expected local DXF adapter override to preserve an explanatory error message");
    }

    auto remoteAdapter = DXFAdapterFactory::CreateDXFPathSourceAdapter(DXFAdapterFactory::AdapterType::REMOTE);
    if (!remoteAdapter) {
        return Fail("failed to create remote DXF adapter");
    }
    if (DXFAdapterFactory::CheckAdapterHealth(remoteAdapter)) {
        return Fail("expected remote DXF adapter stub to report unhealthy instead of falling back");
    }

    const auto remoteValidation = remoteAdapter->ValidateDXFFile("remote_input.dxf");
    if (!remoteValidation.IsError()) {
        return Fail("expected remote DXF validation stub to fail explicitly");
    }
    if (remoteValidation.GetError().GetCode() != ErrorCode::NOT_IMPLEMENTED) {
        return Fail("expected NOT_IMPLEMENTED from remote DXF validation stub");
    }

    std::cout << "workflow planning ingress smoke passed" << std::endl;
    return 0;
}
