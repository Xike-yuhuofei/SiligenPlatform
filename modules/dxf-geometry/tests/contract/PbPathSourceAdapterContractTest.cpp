#include "dxf_geometry/adapters/planning/dxf/PbPathSourceAdapter.h"

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
        return fail("expected adapter contract to fail explicitly for missing/disabled protobuf input");
    }

    const auto& error = result.GetError();
    const std::string message = error.GetMessage();
    if (error.GetCode() == Siligen::Shared::Types::ErrorCode::FILE_NOT_FOUND) {
        if (message.find("disabled.pb") == std::string::npos) {
            return fail("expected protobuf-on adapter contract message to mention the missing .pb file");
        }
        std::cout << "protobuf-on adapter contract passed" << std::endl;
        return 0;
    }

    if (error.GetCode() == Siligen::Shared::Types::ErrorCode::NOT_IMPLEMENTED) {
        if (message.find("SILIGEN_ENABLE_PROTOBUF=OFF") == std::string::npos) {
            return fail("expected disabled protobuf contract message to mention SILIGEN_ENABLE_PROTOBUF=OFF");
        }
        std::cout << "protobuf-off adapter contract passed" << std::endl;
        return 0;
    }

    return fail("expected FILE_NOT_FOUND or NOT_IMPLEMENTED from protobuf adapter contract");
}
