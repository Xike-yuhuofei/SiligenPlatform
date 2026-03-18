# 错误码统一规范（快速参考）

## 统一来源
- 唯一权威定义: `src/shared/types/Error.h` 中的 `ErrorCode`。
- 所有模块返回 `Result` 时必须使用 `ErrorCode`。
- 任何第三方错误码必须在适配层映射为 `ErrorCode`（例如 MultiCard）。
- 自动生成清单: `docs/library/error-codes-list.md`。

## 编码区间
- 通用错误: 1-99
- 连接相关错误: 100-199
- 运动控制错误: 200-299
- 数据持久化错误: 300-399
- 文件解析错误: 400-499
- 线程操作错误: 500-599
- 硬件错误: 1000-1999
- 配置错误: 2000-2999
- 文件上传错误: 2500-2599
- 依赖注入错误: 3000-3999
- CMP 触发错误: 4000-4999
- 序列化错误: 6000-6999
- 适配器错误: 9000-9099
- 网络错误: 9100-9199
- 硬件错误（扩展段）: 9200-9299
- 兜底错误: 9900-9999

## 适配层映射要求
- 外部 SDK 的返回码不得向上层透出，必须映射为 `ErrorCode`。
- 映射函数应在适配层集中管理，并保持可追溯上下文（函数名、操作名）。

示例（MultiCard）:
```cpp
Error MultiCardAdapter::MapMultiCardError(int32_t error_code, const std::string& operation) const {
    std::string formatted_msg = MultiCardErrorCodes::FormatErrorMessage(error_code, operation);
    ErrorCode mapped_code = ErrorCode::HARDWARE_CONNECTION_FAILED;
    if (MultiCardErrorCodes::IsCommunicationError(error_code)) {
        mapped_code = ErrorCode::CONNECTION_FAILED;
    } else if (MultiCardErrorCodes::IsParameterError(error_code)) {
        mapped_code = ErrorCode::INVALID_PARAMETER;
    }
    return Error(mapped_code, formatted_msg, "MultiCardAdapter");
}
```

## 新增错误码规则
- 先确认所属区间，不得占用已有值。
- 命名使用全大写 + 下划线（与 `ErrorCodeToString` 对齐）。
- 必须在 `ErrorCodeToString` 增加对应分支。
- 若为序列化/配置/适配等明确领域，优先放入专用区间，避免泛化到 `UNKNOWN_ERROR`。

## 相关文件
- `src/shared/types/Error.h`
- `src/shared/types/SerializationTypes.h`
- `modules/device-hal/src/drivers/multicard/MultiCardAdapter.cpp`
