#pragma once

#include "domain/diagnostics/value-objects/TestDataTypes.h"
#include "shared/types/Result.h"

#include <string>

/**
 * @brief 工艺/测试结果序列化规范（Domain 权威定义）
 *
 * JSON 字段结构视为对外契约，修改需保证兼容并更新相关测试。
 */
namespace Siligen::Domain::Diagnostics::Serialization {

using Siligen::Shared::Types::Result;
using Siligen::Domain::Diagnostics::ValueObjects::CMPTestData;
using Siligen::Domain::Diagnostics::ValueObjects::DiagnosticResult;
using Siligen::Domain::Diagnostics::ValueObjects::HomingTestData;
using Siligen::Domain::Diagnostics::ValueObjects::InterpolationTestData;
using Siligen::Domain::Diagnostics::ValueObjects::JogTestData;
using Siligen::Domain::Diagnostics::ValueObjects::TriggerTestData;

Result<std::string> SerializeJogParameters(const JogTestData& data) noexcept;
Result<std::string> SerializeJogResult(const JogTestData& data) noexcept;
Result<JogTestData> DeserializeJogTestData(const std::string& parameters_json,
                                           const std::string& result_json) noexcept;

Result<std::string> SerializeHomingParameters(const HomingTestData& data) noexcept;
Result<std::string> SerializeHomingResult(const HomingTestData& data) noexcept;

Result<std::string> SerializeTriggerParameters(const TriggerTestData& data) noexcept;
Result<std::string> SerializeTriggerResult(const TriggerTestData& data) noexcept;

Result<std::string> SerializeCMPParameters(const CMPTestData& data) noexcept;
Result<std::string> SerializeCMPResult(const CMPTestData& data) noexcept;

Result<std::string> SerializeInterpolationParameters(const InterpolationTestData& data) noexcept;
Result<std::string> SerializeInterpolationResult(const InterpolationTestData& data) noexcept;

Result<std::string> SerializeDiagnosticResult(const DiagnosticResult& data) noexcept;

}  // namespace Siligen::Domain::Diagnostics::Serialization
