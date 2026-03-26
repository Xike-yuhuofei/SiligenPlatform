#include "application/usecases/dispensing/UploadFileUseCase.h"
#include "application/services/dxf/DxfPbPreparationService.h"
#include "domain/configuration/ports/IFileStoragePort.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <chrono>
#include <iterator>
#include <string>

namespace {
using Siligen::Application::Services::DXF::DxfPbPreparationService;
using Siligen::Application::UseCases::Dispensing::UploadRequest;
using Siligen::Application::UseCases::Dispensing::UploadFileUseCase;
using Siligen::Domain::Configuration::Ports::FileData;
using Siligen::Domain::Configuration::Ports::IFileStoragePort;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

class TestFileStoragePort final : public IFileStoragePort {
   public:
    explicit TestFileStoragePort(std::filesystem::path base_dir) : base_dir_(std::move(base_dir)) {
        std::filesystem::create_directories(base_dir_);
    }

    Result<std::string> StoreFile(const FileData& file_data, const std::string& filename) override {
        if (file_data.content.empty()) {
            return Result<std::string>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "file content empty", "TestFileStoragePort"));
        }
        std::filesystem::create_directories(base_dir_);
        std::filesystem::path full_path = base_dir_ / filename;
        std::ofstream out(full_path, std::ios::binary);
        if (!out.good()) {
            return Result<std::string>::Failure(
                Error(ErrorCode::FILE_IO_ERROR, "failed to open file", "TestFileStoragePort"));
        }
        out.write(reinterpret_cast<const char*>(file_data.content.data()),
                  static_cast<std::streamsize>(file_data.content.size()));
        out.close();
        return Result<std::string>::Success(full_path.string());
    }

    Result<void> ValidateFile(const FileData&, size_t, const std::vector<std::string>&) override {
        return Result<void>::Success();
    }

    Result<void> DeleteFile(const std::string& filepath) override {
        std::error_code ec;
        std::filesystem::remove(filepath, ec);
        if (ec) {
            return Result<void>::Failure(
                Error(ErrorCode::FILE_IO_ERROR, "delete failed", "TestFileStoragePort"));
        }
        return Result<void>::Success();
    }

    Result<bool> FileExists(const std::string& filepath) override {
        std::error_code ec;
        bool exists = std::filesystem::exists(filepath, ec);
        if (ec) {
            return Result<bool>::Failure(
                Error(ErrorCode::FILE_IO_ERROR, "exists check failed", "TestFileStoragePort"));
        }
        return Result<bool>::Success(exists);
    }

    Result<size_t> GetFileSize(const std::string& filepath) override {
        std::error_code ec;
        auto size = std::filesystem::file_size(filepath, ec);
        if (ec) {
            return Result<size_t>::Failure(
                Error(ErrorCode::FILE_IO_ERROR, "size check failed", "TestFileStoragePort"));
        }
        return Result<size_t>::Success(static_cast<size_t>(size));
    }

   private:
    std::filesystem::path base_dir_;
};

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

std::string QuoteArg(const std::string& value) {
    if (value.find_first_of(" \t\"") == std::string::npos) {
        return value;
    }
    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('"');
    for (char c : value) {
        if (c == '"') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

std::filesystem::path MakeTempDir(const std::string& name) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path = std::filesystem::temp_directory_path() / ("siligen_" + name + "_" + std::to_string(now));
    std::filesystem::create_directories(path);
    return path;
}

std::string MinimalDxf() {
    return "0\nSECTION\n2\nENTITIES\n0\nLINE\n10\n0\n20\n0\n11\n10\n21\n0\n0\nENDSEC\n0\nEOF\n";
}

void WriteTextFile(const std::filesystem::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    out << content;
}

}  // namespace

TEST(UploadFileUseCaseTest, GeneratesPbAfterUpload) {
    const auto base_dir = MakeTempDir("upload_success");

    const auto generator_path = base_dir / "gen_pb.py";
    {
        std::ofstream script(generator_path);
        script << "import pathlib\n";
        script << "import sys\n";
        script << "pathlib.Path(sys.argv[2]).write_bytes(b'pb')\n";
    }

    std::string command = "python " + QuoteArg(generator_path.string()) + " {input} {output}";
    SetEnvVar("SILIGEN_DXF_PB_COMMAND", command);

    UploadRequest request;
    request.original_filename = "unit_test.dxf";
    request.content_type = "application/dxf";
    const std::string minimal_dxf = MinimalDxf();
    request.file_content.assign(minimal_dxf.begin(), minimal_dxf.end());
    request.file_size = request.file_content.size();

    auto storage = std::make_shared<TestFileStoragePort>(base_dir);
    UploadFileUseCase usecase(storage);

    auto result = usecase.Execute(request);
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();

    std::filesystem::path dxf_path(result.Value().filepath);
    std::filesystem::path pb_path = dxf_path;
    pb_path.replace_extension(".pb");

    EXPECT_TRUE(std::filesystem::exists(dxf_path));
    EXPECT_TRUE(std::filesystem::exists(pb_path));

    UnsetEnvVar("SILIGEN_DXF_PB_COMMAND");

    std::error_code ec;
    std::filesystem::remove(dxf_path, ec);
    std::filesystem::remove(pb_path, ec);
    std::filesystem::remove(generator_path, ec);
    std::filesystem::remove_all(base_dir, ec);
}

TEST(DxfPbPreparationServiceTest, SkipsRegenerationWhenUpToDatePbExists) {
    const auto base_dir = MakeTempDir("pb_skip_regen");
    const auto dxf_path = base_dir / "sample.dxf";
    const auto pb_path = base_dir / "sample.pb";
    const auto generator_path = base_dir / "should_not_run.cmd";

    WriteTextFile(dxf_path, MinimalDxf());
    WriteTextFile(pb_path, "cached-pb");
    WriteTextFile(generator_path, "@echo off\nexit /b 9\n");

    SetEnvVar("SILIGEN_DXF_PB_COMMAND", QuoteArg(generator_path.string()) + " {input} {output}");

    DxfPbPreparationService service;
    auto result = service.EnsurePbReady(dxf_path.string());
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();
    EXPECT_EQ(result.Value(), pb_path.string());

    std::ifstream in(pb_path, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(content, "cached-pb");

    UnsetEnvVar("SILIGEN_DXF_PB_COMMAND");

    std::error_code ec;
    std::filesystem::remove_all(base_dir, ec);
}

TEST(DxfPbPreparationServiceTest, RejectsEmptyPbOutput) {
    const auto base_dir = MakeTempDir("pb_empty_output");
    const auto dxf_path = base_dir / "sample.dxf";
    const auto generator_path = base_dir / "write_empty.py";

    WriteTextFile(dxf_path, MinimalDxf());
    WriteTextFile(generator_path,
                  "import pathlib\n"
                  "import sys\n"
                  "pathlib.Path(sys.argv[2]).write_bytes(b'')\n");

    SetEnvVar("SILIGEN_DXF_PB_COMMAND", "python " + QuoteArg(generator_path.string()) + " {input} {output}");

    DxfPbPreparationService service;
    auto result = service.EnsurePbReady(dxf_path.string());
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::FILE_FORMAT_INVALID);

    UnsetEnvVar("SILIGEN_DXF_PB_COMMAND");

    std::error_code ec;
    std::filesystem::remove_all(base_dir, ec);
}

TEST(DxfPbPreparationServiceTest, UsesExternalDxFProjectLauncherWhenAvailable) {
    const auto root_dir = MakeTempDir("external_dxf_project");
    const auto dxf_repo_dir = root_dir / "DXF";
    const auto scripts_dir = dxf_repo_dir / "scripts";
    const auto output_dir = root_dir / "artifacts";
    const auto dxf_path = output_dir / "sample.dxf";
    const auto launcher_path = scripts_dir / "dxf_core_launcher.py";

    std::filesystem::create_directories(scripts_dir);
    std::filesystem::create_directories(output_dir);

    WriteTextFile(
        launcher_path,
        "import pathlib\n"
        "import sys\n"
        "\n"
        "args = sys.argv[1:]\n"
        "assert args[0] == 'engineering-data-dxf-to-pb'\n"
        "output = pathlib.Path(args[args.index('--output') + 1])\n"
        "output.write_bytes(b'external-pb')\n");

    WriteTextFile(dxf_path, MinimalDxf());

    SetEnvVar("SILIGEN_DXF_PROJECT_ROOT", dxf_repo_dir.string());

    DxfPbPreparationService service;
    auto result = service.EnsurePbReady(dxf_path.string());

    UnsetEnvVar("SILIGEN_DXF_PROJECT_ROOT");

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();
    std::filesystem::path pb_path = dxf_path;
    pb_path.replace_extension(".pb");
    ASSERT_TRUE(std::filesystem::exists(pb_path));

    std::ifstream in(pb_path, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(content, "external-pb");

    std::error_code ec;
    std::filesystem::remove_all(root_dir, ec);
}

TEST(DxfPbPreparationServiceTest, RejectsInvalidExternalDxFProjectRoot) {
    const auto base_dir = MakeTempDir("invalid_external_root");
    const auto dxf_path = base_dir / "sample.dxf";

    WriteTextFile(dxf_path, MinimalDxf());
    SetEnvVar("SILIGEN_DXF_PROJECT_ROOT", (base_dir / "missing_dxf_repo").string());

    DxfPbPreparationService service;
    auto result = service.EnsurePbReady(dxf_path.string());
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::FILE_NOT_FOUND);

    UnsetEnvVar("SILIGEN_DXF_PROJECT_ROOT");
    std::error_code ec;
    std::filesystem::remove_all(base_dir, ec);
}

TEST(DxfPbPreparationServiceTest, RejectsCommandOverrideWithoutInputOutputPlaceholders) {
    const auto base_dir = MakeTempDir("invalid_command_override");
    const auto dxf_path = base_dir / "sample.dxf";

    WriteTextFile(dxf_path, MinimalDxf());
    SetEnvVar("SILIGEN_DXF_PB_COMMAND", "python fake_generator.py");

    DxfPbPreparationService service;
    auto result = service.EnsurePbReady(dxf_path.string());
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::CONFIGURATION_ERROR);

    UnsetEnvVar("SILIGEN_DXF_PB_COMMAND");
    std::error_code ec;
    std::filesystem::remove_all(base_dir, ec);
}

TEST(DxfPbPreparationServiceTest, RejectsCommandOverrideWithShellMetaCharacters) {
    const auto base_dir = MakeTempDir("invalid_command_meta");
    const auto dxf_path = base_dir / "sample.dxf";

    WriteTextFile(dxf_path, MinimalDxf());
    SetEnvVar("SILIGEN_DXF_PB_COMMAND", "python fake_generator.py {input} {output} && echo hacked");

    DxfPbPreparationService service;
    auto result = service.EnsurePbReady(dxf_path.string());
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::CONFIGURATION_ERROR);

    UnsetEnvVar("SILIGEN_DXF_PB_COMMAND");
    std::error_code ec;
    std::filesystem::remove_all(base_dir, ec);
}

TEST(DxfPbPreparationServiceTest, DefaultConfigPassesNoStrictR12ToPythonExporter) {
    const auto base_dir = MakeTempDir("default_no_strict_r12");
    const auto dxf_path = base_dir / "sample.dxf";
    const auto script_path = base_dir / "capture_args.py";

    WriteTextFile(dxf_path, MinimalDxf());
    WriteTextFile(
        script_path,
        "import pathlib\n"
        "import sys\n"
        "args = sys.argv[1:]\n"
        "if '--no-strict-r12' not in args:\n"
        "    raise SystemExit(8)\n"
        "output = pathlib.Path(args[args.index('--output') + 1])\n"
        "output.write_bytes(b'default-no-strict')\n");

    SetEnvVar("SILIGEN_DXF_PB_SCRIPT", script_path.string());
    DxfPbPreparationService service;
    auto result = service.EnsurePbReady(dxf_path.string());
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();

    UnsetEnvVar("SILIGEN_DXF_PB_SCRIPT");
    std::error_code ec;
    std::filesystem::remove_all(base_dir, ec);
}

TEST(DxfPbPreparationServiceTest, FindsDefaultPbScriptRelativeToWorkspaceWhenCwdIsNestedBuildDir) {
    const auto base_dir = MakeTempDir("default_script_workspace_lookup");
    const auto workspace_root = base_dir / "workspace";
    const auto nested_cwd = workspace_root / "build" / "bin" / "Debug";
    const auto uploads_dir = workspace_root / "uploads";
    const auto engineering_data_scripts_dir = workspace_root / "scripts" / "engineering-data";
    const auto dxf_path = uploads_dir / "sample.dxf";
    const auto script_path = engineering_data_scripts_dir / "dxf_to_pb.py";

    std::filesystem::create_directories(nested_cwd);
    std::filesystem::create_directories(uploads_dir);
    std::filesystem::create_directories(engineering_data_scripts_dir);

    WriteTextFile(dxf_path, MinimalDxf());
    WriteTextFile(
        script_path,
        "import pathlib\n"
        "import sys\n"
        "args = sys.argv[1:]\n"
        "output = pathlib.Path(args[args.index('--output') + 1])\n"
        "output.write_bytes(b'workspace-script-pb')\n");

    UnsetEnvVar("SILIGEN_DXF_PB_SCRIPT");
    UnsetEnvVar("SILIGEN_DXF_PB_COMMAND");

    std::error_code ec;
    const auto original_cwd = std::filesystem::current_path(ec);
    ASSERT_FALSE(ec);
    std::filesystem::current_path(nested_cwd, ec);
    ASSERT_FALSE(ec);

    DxfPbPreparationService service;
    auto result = service.EnsurePbReady(dxf_path.string());

    std::filesystem::current_path(original_cwd, ec);
    ASSERT_FALSE(ec);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();
    std::filesystem::path pb_path = dxf_path;
    pb_path.replace_extension(".pb");
    ASSERT_TRUE(std::filesystem::exists(pb_path));

    std::ifstream in(pb_path, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(content, "workspace-script-pb");

    std::filesystem::remove_all(base_dir, ec);
}

TEST(DxfPbPreparationServiceTest, PrefersCanonicalEngineeringDataPythonEnv) {
    const auto base_dir = MakeTempDir("canonical_python_env");
    const auto dxf_path = base_dir / "sample.dxf";
    const auto script_path = base_dir / "emit_pb.py";

    WriteTextFile(dxf_path, MinimalDxf());
    WriteTextFile(
        script_path,
        "import pathlib\n"
        "import sys\n"
        "args = sys.argv[1:]\n"
        "output = pathlib.Path(args[args.index('--output') + 1])\n"
        "output.write_bytes(b'pb-from-canonical-env')\n");

    SetEnvVar("SILIGEN_ENGINEERING_DATA_PYTHON", "python");
    SetEnvVar("SILIGEN_DXF_PB_PYTHON", "python_binary_that_must_not_be_used");
    SetEnvVar("SILIGEN_DXF_PB_SCRIPT", script_path.string());

    DxfPbPreparationService service;
    auto result = service.EnsurePbReady(dxf_path.string());
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();

    UnsetEnvVar("SILIGEN_DXF_PB_SCRIPT");
    UnsetEnvVar("SILIGEN_DXF_PB_PYTHON");
    UnsetEnvVar("SILIGEN_ENGINEERING_DATA_PYTHON");
    std::error_code ec;
    std::filesystem::remove_all(base_dir, ec);
}

TEST(UploadFileUseCaseTest, CleansUpArtifactsWhenPbGenerationFails) {
    const auto base_dir = MakeTempDir("upload_failure_cleanup");
    const auto generator_path = base_dir / "fail_pb.py";
    WriteTextFile(generator_path,
                  "raise SystemExit(7)\n");

    SetEnvVar("SILIGEN_DXF_PB_COMMAND", "python " + QuoteArg(generator_path.string()) + " {input} {output}");

    UploadRequest request;
    request.original_filename = "unit_test.dxf";
    request.content_type = "application/dxf";
    const std::string minimal_dxf = MinimalDxf();
    request.file_content.assign(minimal_dxf.begin(), minimal_dxf.end());
    request.file_size = request.file_content.size();

    auto storage = std::make_shared<TestFileStoragePort>(base_dir);
    UploadFileUseCase usecase(storage);

    auto result = usecase.Execute(request);
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::COMMAND_FAILED);

    size_t dxf_count = 0;
    size_t pb_count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(base_dir)) {
        if (entry.path().extension() == ".dxf") {
            ++dxf_count;
        }
        if (entry.path().extension() == ".pb") {
            ++pb_count;
        }
    }
    EXPECT_EQ(dxf_count, 0u);
    EXPECT_EQ(pb_count, 0u);

    UnsetEnvVar("SILIGEN_DXF_PB_COMMAND");

    std::error_code ec;
    std::filesystem::remove_all(base_dir, ec);
}


