#include <filesystem>
#include <fstream>
#include <iostream>
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

    if (const int result = ExpectMissing(
            root / "modules/workflow/tests/process-runtime-core",
            "legacy workflow tests root");
        result != 0) {
        return result;
    }

    if (const int result = ExpectMissing(
            root / "modules/workflow/application/usecases/motion/runtime/MotionRuntimeAssemblyFactory.h",
            "workflow motion runtime assembly header");
        result != 0) {
        return result;
    }

    if (const int result = ExpectMissing(
            root / "modules/workflow/application/usecases/motion/runtime/MotionRuntimeAssemblyFactory.cpp",
            "workflow motion runtime assembly source");
        result != 0) {
        return result;
    }

    if (const int result = ExpectMissing(
            root / "modules/workflow/domain/include/domain/motion/domain-services/MotionControlServiceImpl.h",
            "workflow runtime concrete shim header");
        result != 0) {
        return result;
    }

    if (const int result = ExpectMissing(
            root / "modules/workflow/domain/domain/motion/domain-services/MotionStatusServiceImpl.h",
            "workflow private runtime concrete shim header");
        result != 0) {
        return result;
    }

    const auto workflowApplicationText = ReadText(root / "modules/workflow/application/CMakeLists.txt");
    const auto workflowUnitTestsText = ReadText(root / "modules/workflow/tests/unit/CMakeLists.txt");
    const auto initializeSystemHeader =
        ReadText(root / "modules/workflow/application/include/application/usecases/system/InitializeSystemUseCase.h");
    const auto initializeSystemSource =
        ReadText(root / "modules/workflow/application/usecases/system/InitializeSystemUseCase.cpp");

    if (workflowApplicationText.empty() || workflowUnitTestsText.empty() ||
        initializeSystemHeader.empty() || initializeSystemSource.empty()) {
        return Fail("failed to read workflow cutover smoke inputs");
    }

    if (const int result = ExpectNotContains(
            workflowApplicationText,
            "siligen_runtime_execution_application_public",
            "workflow application CMake");
        result != 0) {
        return result;
    }

    if (const int result = ExpectNotContains(
            workflowApplicationText,
            "MotionRuntimeAssemblyFactory.cpp",
            "workflow application CMake");
        result != 0) {
        return result;
    }

    if (const int result = ExpectNotContains(
            workflowUnitTestsText,
            "process_runtime_core_",
            "workflow unit tests CMake");
        result != 0) {
        return result;
    }

    if (const int result = ExpectNotContains(
            workflowUnitTestsText,
            "HomeAxesUseCaseTest.cpp",
            "workflow unit tests CMake");
        result != 0) {
        return result;
    }

    if (const int result = ExpectContains(
            initializeSystemHeader,
            "IHomeAxesExecutionPort",
            "InitializeSystemUseCase header");
        result != 0) {
        return result;
    }

    if (const int result = ExpectContains(
            initializeSystemSource,
            "IHomeAxesExecutionPort",
            "InitializeSystemUseCase source");
        result != 0) {
        return result;
    }

    if (const int result = ExpectNotContains(
            initializeSystemSource,
            "runtime_execution/application/usecases/motion/homing/HomeAxesUseCase.h",
            "InitializeSystemUseCase source");
        result != 0) {
        return result;
    }

    std::cout << "workflow boundary cutover smoke passed" << std::endl;
    return 0;
}
