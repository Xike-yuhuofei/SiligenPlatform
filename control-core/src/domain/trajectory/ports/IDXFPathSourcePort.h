#pragma once

#include "domain/trajectory/value-objects/Primitive.h"
#include "shared/types/Result.h"
#include <string>
#include <vector>

namespace Siligen::Domain::Trajectory::Ports {

using Siligen::Domain::Trajectory::ValueObjects::Primitive;
using Siligen::Shared::Types::Result;

/**
 * @brief DXF专用路径源端口接口
 * 
 * 该接口专门处理DXF文件格式的路径加载，与通用IPathSourcePort形成职责分离。
 * DXF相关功能迁移到独立项目后，此接口将作为兼容层边界。
 */
struct DXFValidationResult {
    bool is_valid = false;
    std::string format_version;
    std::string error_details;
    int entity_count = 0;
    bool requires_preprocessing = false;
};

struct DXFPathSourceResult {
    bool success = false;
    std::string error_message;
    std::vector<Primitive> primitives;
    std::string source_format;  // R12, R14, 2000, etc.
    bool was_preprocessed = false;
    std::string preprocessing_log;
};

class IDXFPathSourcePort {
public:
    virtual ~IDXFPathSourcePort() = default;

    /**
     * @brief 加载并预处理DXF文件
     * 
     * 与通用LoadFromFile不同，此方法专门处理DXF格式，
     * 包含自动格式检测和预处理逻辑。
     */
    virtual Result<DXFPathSourceResult> LoadDXFFile(const std::string& filepath) = 0;

    /**
     * @brief 验证DXF文件格式和完整性
     * 
     * 在加载前进行格式验证，避免无效文件影响执行链路。
     */
    virtual Result<DXFValidationResult> ValidateDXFFile(const std::string& filepath) = 0;

    /**
     * @brief 获取支持的DXF格式列表
     * 
     * 返回适配器支持的DXF版本和格式，用于UI展示和格式选择。
     */
    virtual std::vector<std::string> GetSupportedFormats() = 0;

    /**
     * @brief 检查是否需要预处理
     * 
     * 快速检查文件是否需要预处理，避免不必要的处理开销。
     */
    virtual Result<bool> RequiresPreprocessing(const std::string& filepath) = 0;
};

} // namespace Siligen::Domain::Trajectory::Ports