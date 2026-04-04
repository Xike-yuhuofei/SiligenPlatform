#pragma once

#include "process_path/contracts/IPathSourcePort.h"
#include "process_path/contracts/IDXFPathSourcePort.h"
#include "PbPathSourceAdapter.h"

#include <memory>
#include <string>

namespace Siligen::Infrastructure::Adapters::Parsing {

using Siligen::Domain::Trajectory::Ports::PathSourceResult;
using Siligen::Domain::Trajectory::Ports::DXFPathSourceResult;
using Siligen::Domain::Trajectory::Ports::DXFValidationResult;
using Siligen::Shared::Types::Result;

/**
 * @brief 自动路径源适配器 - 支持通用和DXF专用接口
 * 
 * 该类同时实现IPathSourcePort和IDXFPathSourcePort接口，
 * 确保在迁移过程中保持向后兼容性。
 * 未来DXF功能完全迁移后，可移除通用接口实现。
 */
class AutoPathSourceAdapter : 
    public Siligen::Domain::Trajectory::Ports::IPathSourcePort,
    public Siligen::Domain::Trajectory::Ports::IDXFPathSourcePort {
public:
    AutoPathSourceAdapter();
    ~AutoPathSourceAdapter() override = default;

    // 实现通用IPathSourcePort接口（向后兼容）
    Result<PathSourceResult> LoadFromFile(const std::string& filepath) override;

    // 实现专用IDXFPathSourcePort接口（新设计）
    Result<DXFPathSourceResult> LoadDXFFile(const std::string& filepath) override;
    Result<DXFValidationResult> ValidateDXFFile(const std::string& filepath) override;
    std::vector<std::string> GetSupportedFormats() override;
    Result<bool> RequiresPreprocessing(const std::string& filepath) override;

private:
    std::shared_ptr<PbPathSourceAdapter> pb_adapter_;
    
    // DXF预处理相关方法
    Result<std::string> PreprocessDXFFile(const std::string& dxf_path);
    Result<bool> CheckPBFileExists(const std::string& dxf_path);
    Result<DXFValidationResult> AnalyzeDXFFile(const std::string& filepath);
    
    // dxf-pipeline服务调用相关方法
    bool CheckDXFPipelineServiceAvailable();
    Result<bool> CallDXFPipelineService(const std::string& dxf_path, const std::string& pb_path);
    Result<bool> CallCommandLineService(const std::string& dxf_path, const std::string& pb_path);
    Result<bool> CallHttpService(const std::string& dxf_path, const std::string& pb_path);
};

} // namespace Siligen::Infrastructure::Adapters::Parsing
