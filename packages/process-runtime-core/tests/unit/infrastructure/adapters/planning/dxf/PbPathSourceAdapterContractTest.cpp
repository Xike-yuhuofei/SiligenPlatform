#include "infrastructure/adapters/planning/dxf/PbPathSourceAdapter.h"

#include "shared/types/Error.h"

#include <iostream>
#include <string>

namespace {

int fail(const std::string& message) {
    std::cerr << message << std::endl;
    return 1;
}

}  // namespace

int main() {
    Siligen::Infrastructure::Adapters::Parsing::PbPathSourceAdapter adapter;
    const auto result = adapter.LoadFromFile("disabled.pb");

    if (!result.IsError()) {
        return fail("expected protobuf-off adapter contract to fail explicitly");
    }

    const auto& error = result.GetError();
    if (error.GetCode() != Siligen::Shared::Types::ErrorCode::NOT_IMPLEMENTED) {
        return fail("expected NOT_IMPLEMENTED when protobuf path loading is disabled");
    }

    const std::string message = error.GetMessage();
    if (message.find("SILIGEN_ENABLE_PROTOBUF=OFF") == std::string::npos) {
        return fail("expected disabled protobuf contract message to mention SILIGEN_ENABLE_PROTOBUF=OFF");
    }

    std::cout << "protobuf-off adapter contract passed" << std::endl;
    return 0;
}
