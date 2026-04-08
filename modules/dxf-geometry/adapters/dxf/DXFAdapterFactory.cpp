#include "dxf_geometry/adapters/planning/dxf/DXFAdapterFactory.h"

namespace Siligen::Infrastructure::Adapters::Parsing {

using Siligen::Domain::Trajectory::Ports::DXFPathSourceResult;
using Siligen::Domain::Trajectory::Ports::DXFValidationResult;
using Siligen::Domain::Trajectory::Ports::IDXFPathSourcePort;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

namespace {

class MockDXFPathSourceAdapter final : public IDXFPathSourcePort {
   public:
    Result<DXFPathSourceResult> LoadDXFFile(const std::string& filepath) override {
        if (filepath.empty()) {
            return Result<DXFPathSourceResult>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "filepath must not be empty", "MockDXFPathSourceAdapter"));
        }

        DXFPathSourceResult result;
        result.success = true;
        result.source_format = "MOCK_DXF";
        result.preprocessing_log = "mock adapter bypassed preprocessing";
        return Result<DXFPathSourceResult>::Success(std::move(result));
    }

    Result<DXFValidationResult> ValidateDXFFile(const std::string& filepath) override {
        if (filepath.empty()) {
            return Result<DXFValidationResult>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "filepath must not be empty", "MockDXFPathSourceAdapter"));
        }

        DXFValidationResult result;
        result.is_valid = true;
        result.format_version = "MOCK";
        result.entity_count = 0;
        result.requires_preprocessing = false;
        return Result<DXFValidationResult>::Success(std::move(result));
    }

    std::vector<std::string> GetSupportedFormats() override {
        return {"MOCK_DXF"};
    }

    Result<bool> RequiresPreprocessing(const std::string& filepath) override {
        if (filepath.empty()) {
            return Result<bool>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "filepath must not be empty", "MockDXFPathSourceAdapter"));
        }
        return Result<bool>::Success(false);
    }
};

class RemoteDXFPathSourceAdapterStub final : public IDXFPathSourcePort {
   public:
    Result<DXFPathSourceResult> LoadDXFFile(const std::string&) override {
        return Result<DXFPathSourceResult>::Failure(
            Error(ErrorCode::NOT_IMPLEMENTED,
                  "remote DXF adapter is not implemented yet",
                  "RemoteDXFPathSourceAdapterStub"));
    }

    Result<DXFValidationResult> ValidateDXFFile(const std::string&) override {
        return Result<DXFValidationResult>::Failure(
            Error(ErrorCode::NOT_IMPLEMENTED,
                  "remote DXF adapter is not implemented yet",
                  "RemoteDXFPathSourceAdapterStub"));
    }

    std::vector<std::string> GetSupportedFormats() override {
        return {};
    }

    Result<bool> RequiresPreprocessing(const std::string&) override {
        return Result<bool>::Failure(
            Error(ErrorCode::NOT_IMPLEMENTED,
                  "remote DXF adapter is not implemented yet",
                  "RemoteDXFPathSourceAdapterStub"));
    }
};

}  // namespace

DXFAdapterFactory::AdapterType DXFAdapterFactory::current_adapter_type_ = DXFAdapterFactory::AdapterType::LOCAL;

Siligen::Shared::Types::Result<std::shared_ptr<IDXFPathSourcePort>> DXFAdapterFactory::CreateDXFPathSourceAdapter(
    AdapterType type) {
    current_adapter_type_ = type;

    switch (type) {
        case AdapterType::LOCAL:
            return Siligen::Shared::Types::Result<std::shared_ptr<IDXFPathSourcePort>>::Success(CreateLocalAdapter());
        case AdapterType::REMOTE:
            return Siligen::Shared::Types::Result<std::shared_ptr<IDXFPathSourcePort>>::Failure(
                Siligen::Shared::Types::Error(
                    Siligen::Shared::Types::ErrorCode::NOT_IMPLEMENTED,
                    "Remote DXF adapter is not available",
                    "DXFAdapterFactory"));
        case AdapterType::MOCK:
            return Siligen::Shared::Types::Result<std::shared_ptr<IDXFPathSourcePort>>::Success(CreateMockAdapter());
        default:
            return Siligen::Shared::Types::Result<std::shared_ptr<IDXFPathSourcePort>>::Failure(
                Siligen::Shared::Types::Error(
                    Siligen::Shared::Types::ErrorCode::CONFIGURATION_ERROR,
                    "Unknown DXF adapter type",
                    "DXFAdapterFactory"));
    }
}

bool DXFAdapterFactory::CheckAdapterHealth(const std::shared_ptr<IDXFPathSourcePort>& adapter) {
    if (!adapter) {
        return false;
    }

    switch (current_adapter_type_) {
        case AdapterType::LOCAL:
        case AdapterType::MOCK:
            return true;
        case AdapterType::REMOTE:
            return false;
        default:
            return false;
    }
}

DXFAdapterFactory::AdapterType DXFAdapterFactory::GetCurrentAdapterType() {
    return current_adapter_type_;
}

std::shared_ptr<IDXFPathSourcePort> DXFAdapterFactory::CreateLocalAdapter() {
    return std::make_shared<AutoPathSourceAdapter>();
}

std::shared_ptr<IDXFPathSourcePort> DXFAdapterFactory::CreateRemoteAdapter() {
    return std::make_shared<RemoteDXFPathSourceAdapterStub>();
}

std::shared_ptr<IDXFPathSourcePort> DXFAdapterFactory::CreateMockAdapter() {
    return std::make_shared<MockDXFPathSourceAdapter>();
}

}  // namespace Siligen::Infrastructure::Adapters::Parsing
