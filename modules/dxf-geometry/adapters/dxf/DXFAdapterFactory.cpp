#include "DXFAdapterFactory.h"

namespace Siligen::Infrastructure::Adapters::Parsing {

using Siligen::Domain::Trajectory::Ports::IDXFPathSourcePort;
using Siligen::Domain::Trajectory::Ports::DXFPathSourceResult;
using Siligen::Domain::Trajectory::Ports::DXFValidationResult;
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

// 静态成员初始化
DXFAdapterFactory::AdapterType DXFAdapterFactory::current_adapter_type_ = DXFAdapterFactory::AdapterType::LOCAL;

std::shared_ptr<IDXFPathSourcePort> DXFAdapterFactory::CreateDXFPathSourceAdapter(AdapterType type) {
    current_adapter_type_ = type;
    
    switch (type) {
        case AdapterType::LOCAL:
            return CreateLocalAdapter();
        case AdapterType::REMOTE:
            return CreateRemoteAdapter();
        case AdapterType::MOCK:
            return CreateMockAdapter();
        default:
            // 默认回退到本地适配器
            return CreateLocalAdapter();
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
    // 创建AutoPathSourceAdapter实例，它实现了IDXFPathSourcePort接口
    return std::make_shared<AutoPathSourceAdapter>();
}

std::shared_ptr<IDXFPathSourcePort> DXFAdapterFactory::CreateRemoteAdapter() {
    // 显式返回未实现stub，避免误导为本地适配器可替代远程能力。
    return std::make_shared<RemoteDXFPathSourceAdapterStub>();
}

std::shared_ptr<IDXFPathSourcePort> DXFAdapterFactory::CreateMockAdapter() {
    return std::make_shared<MockDXFPathSourceAdapter>();
}

} // namespace Siligen::Infrastructure::Adapters::Parsing
