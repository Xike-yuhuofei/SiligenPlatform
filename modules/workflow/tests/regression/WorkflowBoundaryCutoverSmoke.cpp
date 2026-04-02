#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>

namespace {

int Fail(const std::string& message) {
    std::cerr << message << std::endl;
    return 1;
}

std::filesystem::path RepoRoot() {
    auto current = std::filesystem::absolute(__FILE__).parent_path();
    while (!current.empty()) {
        if (std::filesystem::exists(current / "modules/workflow/CMakeLists.txt") &&
            std::filesystem::exists(current / "tests/contracts/test_bridge_exit_contract.py")) {
            return current;
        }

        const auto parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }

    return {};
}

std::string ReadText(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return {};
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string ExtractBlock(const std::string& text, const char* pattern) {
    const std::regex regex(pattern, std::regex::ECMAScript);
    std::smatch captures;
    if (!std::regex_search(text, captures, regex) || captures.size() < 2) {
        return {};
    }
    return captures[1].str();
}

int ExpectExists(const std::filesystem::path& path, const std::string& label) {
    if (!std::filesystem::exists(path)) {
        return Fail(label + " must exist: " + path.string());
    }
    return 0;
}

int ExpectMissing(const std::filesystem::path& path, const std::string& label) {
    if (std::filesystem::exists(path)) {
        return Fail(label + " must be removed: " + path.string());
    }
    return 0;
}

int ExpectContains(const std::string& text, const std::string& needle, const std::string& label) {
    if (text.find(needle) == std::string::npos) {
        return Fail(label + " must contain: " + needle);
    }
    return 0;
}

int ExpectNotContains(const std::string& text, const std::string& needle, const std::string& label) {
    if (text.find(needle) != std::string::npos) {
        return Fail(label + " must not contain: " + needle);
    }
    return 0;
}

}  // namespace

int main() {
    const auto root = RepoRoot();
    if (root.empty()) {
        return Fail("failed to resolve repository root for workflow boundary cutover smoke");
    }

    const auto motion_runtime_header =
        root / "modules/workflow/application/usecases/motion/runtime/MotionRuntimeAssemblyFactory.h";
    const auto motion_runtime_source =
        root / "modules/workflow/application/usecases/motion/runtime/MotionRuntimeAssemblyFactory.cpp";
    const auto public_io_header =
        root / "modules/workflow/domain/include/domain/motion/ports/IIOControlPort.h";
    const auto public_runtime_header =
        root / "modules/workflow/domain/include/domain/motion/ports/IMotionRuntimePort.h";
    const auto initialize_system_header =
        root / "modules/workflow/application/include/application/usecases/system/InitializeSystemUseCase.h";
    const auto initialize_system_source =
        root / "modules/workflow/application/usecases/system/InitializeSystemUseCase.cpp";
    const auto workflow_application_cmake = root / "modules/workflow/application/CMakeLists.txt";
    const auto workflow_unit_tests_cmake = root / "modules/workflow/tests/unit/CMakeLists.txt";

    for (const auto& item : {
             std::pair{motion_runtime_header, "workflow motion runtime assembly header"},
             std::pair{motion_runtime_source, "workflow motion runtime assembly source"},
             std::pair{public_io_header, "workflow public IO compatibility header"},
             std::pair{public_runtime_header, "workflow public motion runtime compatibility header"},
             std::pair{initialize_system_header, "InitializeSystemUseCase header"},
             std::pair{initialize_system_source, "InitializeSystemUseCase source"},
             std::pair{workflow_application_cmake, "workflow application CMake"},
             std::pair{workflow_unit_tests_cmake, "workflow unit tests CMake"},
         }) {
        if (const int result = ExpectExists(item.first, item.second); result != 0) {
            return result;
        }
    }

    if (const int result = ExpectMissing(
            root / "modules/workflow/tests/process-runtime-core",
            "legacy workflow tests root");
        result != 0) {
        return result;
    }

    const auto motion_runtime_header_text = ReadText(motion_runtime_header);
    const auto motion_runtime_source_text = ReadText(motion_runtime_source);
    const auto public_io_header_text = ReadText(public_io_header);
    const auto public_runtime_header_text = ReadText(public_runtime_header);
    const auto initialize_system_header_text = ReadText(initialize_system_header);
    const auto initialize_system_source_text = ReadText(initialize_system_source);
    const auto workflow_application_text = ReadText(workflow_application_cmake);
    const auto workflow_unit_tests_text = ReadText(workflow_unit_tests_cmake);

    if (motion_runtime_header_text.empty() || motion_runtime_source_text.empty() ||
        public_io_header_text.empty() || public_runtime_header_text.empty() ||
        initialize_system_header_text.empty() || initialize_system_source_text.empty() ||
        workflow_application_text.empty() || workflow_unit_tests_text.empty()) {
        return Fail("failed to read workflow cutover smoke inputs");
    }

    if (const int result = ExpectContains(
            motion_runtime_header_text,
            "runtime_execution/contracts/motion/IMotionRuntimePort.h",
            "workflow motion runtime assembly header");
        result != 0) {
        return result;
    }

    if (const int result = ExpectContains(
            motion_runtime_source_text,
            "std::make_unique<Homing::HomeAxesUseCase>",
            "workflow motion runtime assembly source");
        result != 0) {
        return result;
    }

    if (const int result = ExpectContains(
            public_io_header_text,
            "runtime_execution/contracts/motion/IIOControlPort.h",
            "workflow public IO compatibility header");
        result != 0) {
        return result;
    }

    if (const int result = ExpectContains(
            public_runtime_header_text,
            "runtime_execution/contracts/motion/IMotionRuntimePort.h",
            "workflow public motion runtime compatibility header");
        result != 0) {
        return result;
    }

    if (const int result = ExpectContains(
            initialize_system_header_text,
            "Application::UseCases::Motion::Homing::HomeAxesUseCase",
            "InitializeSystemUseCase header");
        result != 0) {
        return result;
    }

    if (const int result = ExpectContains(
            initialize_system_source_text,
            "application/usecases/motion/homing/HomeAxesUseCase.h",
            "InitializeSystemUseCase source");
        result != 0) {
        return result;
    }

    if (const int result = ExpectNotContains(
            initialize_system_header_text,
            "IHomeAxesExecutionPort",
            "InitializeSystemUseCase header");
        result != 0) {
        return result;
    }

    if (const int result = ExpectNotContains(
            initialize_system_source_text,
            "IHomeAxesExecutionPort",
            "InitializeSystemUseCase source");
        result != 0) {
        return result;
    }

    const auto headers_block = ExtractBlock(
        workflow_application_text,
        R"(target_link_libraries\(siligen_workflow_application_headers INTERFACE([\s\S]*?)target_compile_features\(siligen_workflow_application_headers)");
    const auto motion_block = ExtractBlock(
        workflow_application_text,
        R"(target_link_libraries\(siligen_application_motion PUBLIC([\s\S]*?)target_compile_features\(siligen_application_motion)");

    if (headers_block.empty()) {
        return Fail("workflow application headers target block must be present");
    }
    if (motion_block.empty()) {
        return Fail("workflow application motion target block must be present");
    }

    if (const int result = ExpectContains(
            workflow_application_text,
            "siligen_runtime_execution_runtime_contracts",
            "workflow application CMake");
        result != 0) {
        return result;
    }

    if (const int result = ExpectContains(
            workflow_application_text,
            "MotionRuntimeAssemblyFactory.cpp",
            "workflow application CMake");
        result != 0) {
        return result;
    }

    if (const int result = ExpectNotContains(
            headers_block,
            "siligen_runtime_execution_application_public",
            "workflow application headers target");
        result != 0) {
        return result;
    }

    if (const int result = ExpectNotContains(
            motion_block,
            "siligen_runtime_execution_application_public",
            "workflow application motion target");
        result != 0) {
        return result;
    }

    for (const auto& forbidden : {
             "process_runtime_core_",
             "modules/runtime-execution/application/include",
             "modules/runtime-execution/adapters/device/include",
             "HomeAxesUseCaseTest.cpp",
         }) {
        if (const int result = ExpectNotContains(
                workflow_unit_tests_text,
                forbidden,
                "workflow unit tests CMake");
            result != 0) {
            return result;
        }
    }

    std::cout << "workflow boundary cutover smoke passed" << std::endl;
    return 0;
}
