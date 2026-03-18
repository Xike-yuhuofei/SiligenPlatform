# DXF-Pipeline 迁移实施计划

## 项目概述

将 `control-core` 中的 DXF 预处理功能迁移到 `dxf-pipeline` 项目，建立统一的服务接口。

## 实施步骤

### 第一步：在 dxf-pipeline 中创建服务模块

#### 1.1 创建预处理服务接口 (`dxf-pipeline/src/dxf_pipeline/services/dxf_preprocessing.py`)

```python
#!/usr/bin/env python3
"""DXF预处理服务 - 统一处理DXF文件转换和预处理"""

import os
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional, Dict, Any
import ezdxf

@dataclass
class PreprocessingResult:
    """预处理结果"""
    success: bool
    output_path: str
    error_message: Optional[str] = None
    processing_time_ms: int = 0
    entities_processed: int = 0

@dataclass 
class ValidationResult:
    """验证结果"""
    is_valid: bool
    format_version: str
    error_details: Optional[str] = None
    entity_count: int = 0
    requires_preprocessing: bool = False

@dataclass
class PreprocessingConfig:
    """预处理配置"""
    chordal_tolerance: float = 0.005
    max_segment_length: float = 5.0
    overkill_tolerance: float = 0.001
    angular_tolerance: float = 0.1
    min_segment_length: float = 0.0
    output_format: str = "pb"  # pb, json, etc.

class DXFPreprocessingService:
    """DXF预处理服务"""
    
    def __init__(self):
        self.supported_formats = ["R12", "R14", "2000", "2004", "2007", "2010"]
    
    def convert_to_r12(self, input_path: str, output_path: str) -> PreprocessingResult:
        """转换DXF到R12格式"""
        import time
        start_time = time.time()
        
        try:
            # 实现DXF到R12的转换逻辑
            result = self._convert_dxf_internal(input_path, output_path)
            
            processing_time = int((time.time() - start_time) * 1000)
            return PreprocessingResult(
                success=True,
                output_path=output_path,
                processing_time_ms=processing_time,
                entities_processed=result.get('entities_processed', 0)
            )
            
        except Exception as e:
            processing_time = int((time.time() - start_time) * 1000)
            return PreprocessingResult(
                success=False,
                output_path="",
                error_message=str(e),
                processing_time_ms=processing_time
            )
    
    def preprocess_dxf(self, dxf_path: str, config: PreprocessingConfig) -> PreprocessingResult:
        """预处理DXF文件"""
        # 实现预处理逻辑
        pass
    
    def validate_dxf(self, file_path: str) -> ValidationResult:
        """验证DXF文件格式"""
        try:
            doc = ezdxf.readfile(file_path)
            
            return ValidationResult(
                is_valid=True,
                format_version=doc.dxfversion,
                entity_count=len(doc.modelspace()),
                requires_preprocessing=self._requires_preprocessing(doc)
            )
            
        except Exception as e:
            return ValidationResult(
                is_valid=False,
                format_version="Unknown",
                error_details=str(e)
            )
    
    def get_supported_formats(self) -> list:
        """获取支持的格式列表"""
        return self.supported_formats
    
    def _convert_dxf_internal(self, input_path: str, output_path: str) -> Dict[str, Any]:
        """内部DXF转换实现"""
        # 这里实现具体的转换逻辑
        # 可以复用现有的 dxf_to_pb.py 中的功能
        return {"entities_processed": 100}  # 示例返回值
    
    def _requires_preprocessing(self, doc) -> bool:
        """判断是否需要预处理"""
        # 基于文档内容判断是否需要预处理
        return len(doc.modelspace()) > 1000  # 示例逻辑
```

#### 1.2 创建迁移适配器 (`dxf-pipeline/src/dxf_pipeline/services/migration_adapter.py`)

```python
#!/usr/bin/env python3
"""迁移适配器 - 提供与control-core兼容的接口"""

import json
import subprocess
from typing import Dict, Any
from .dxf_preprocessing import DXFPreprocessingService, PreprocessingResult, ValidationResult

class MigrationAdapter:
    """迁移适配器"""
    
    def __init__(self):
        self.service = DXFPreprocessingService()
    
    def handle_command_line_request(self, args: list) -> Dict[str, Any]:
        """处理命令行请求"""
        if len(args) < 3:
            return self._create_error_response("参数不足，需要输入文件和输出文件路径")
        
        input_path = args[1]
        output_path = args[2]
        
        try:
            # 验证文件
            validation = self.service.validate_dxf(input_path)
            if not validation.is_valid:
                return self._create_error_response(f"文件验证失败: {validation.error_details}")
            
            # 执行转换
            result = self.service.convert_to_r12(input_path, output_path)
            
            if result.success:
                return self._create_success_response({
                    "output_path": result.output_path,
                    "processing_time": result.processing_time_ms,
                    "entities_processed": result.entities_processed
                })
            else:
                return self._create_error_response(result.error_message)
                
        except Exception as e:
            return self._create_error_response(f"处理过程中发生错误: {str(e)}")
    
    def _create_success_response(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """创建成功响应"""
        return {
            "status": "success",
            "data": data
        }
    
    def _create_error_response(self, message: str) -> Dict[str, Any]:
        """创建错误响应"""
        return {
            "status": "error", 
            "error": message
        }
```

#### 1.3 创建命令行入口点 (`dxf-pipeline/scripts/convert_dxf_to_r12.py`)

```python
#!/usr/bin/env python3
"""DXF到R12转换命令行工具 - 迁移自control-core"""

import os
import sys

# 添加项目路径
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

from dxf_pipeline.services.migration_adapter import MigrationAdapter

def main():
    """主函数"""
    adapter = MigrationAdapter()
    
    # 处理命令行参数
    response = adapter.handle_command_line_request(sys.argv)
    
    # 输出结果
    if response["status"] == "success":
        data = response["data"]
        print(f"转换成功!")
        print(f"输出文件: {data['output_path']}")
        print(f"处理时间: {data['processing_time']}ms")
        print(f"处理实体: {data['entities_processed']}个")
        sys.exit(0)
    else:
        print(f"错误: {response['error']}")
        sys.exit(1)

if __name__ == "__main__":
    main()
```

### 第二步：更新项目配置

#### 2.1 更新 requirements.txt

确保包含必要的依赖：

```txt
ezdxf>=1.0.0
protobuf>=4.0.0
flask>=2.0.0  # 如果实现HTTP服务
```

#### 2.2 更新 pyproject.toml

添加新的入口点：

```toml
[project.scripts]
convert_dxf_to_r12 = "dxf_pipeline.cli.convert_dxf_to_r12:main"
dxf_preprocessing_service = "dxf_pipeline.services.dxf_preprocessing:main"
```

### 第三步：测试和验证

#### 3.1 创建测试脚本 (`dxf-pipeline/scripts/test_migration.py`)

```python
#!/usr/bin/env python3
"""迁移功能测试脚本"""

import os
import sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

from dxf_pipeline.services.dxf_preprocessing import DXFPreprocessingService

def test_basic_functionality():
    """测试基本功能"""
    service = DXFPreprocessingService()
    
    # 测试支持的格式
    formats = service.get_supported_formats()
    print(f"支持的格式: {formats}")
    
    # 测试服务初始化
    print("服务初始化成功")
    
if __name__ == "__main__":
    test_basic_functionality()
    print("基本功能测试完成")
```

## 实施时间表

### 第一阶段（1-2天）
- [ ] 创建服务模块结构
- [ ] 实现基本接口
- [ ] 创建命令行工具

### 第二阶段（1天）
- [ ] 测试基本功能
- [ ] 验证与control-core的兼容性
- [ ] 更新项目配置

### 第三阶段（1天）
- [ ] 性能测试和优化
- [ ] 文档更新
- [ ] 部署验证

## 风险缓解

### 技术风险
1. **依赖兼容性**：确保ezdxf版本兼容
2. **文件格式支持**：测试各种DXF版本
3. **性能问题**：大文件处理性能优化

### 迁移风险
1. **向后兼容**：保持与现有工具的兼容性
2. **错误处理**：完善的错误处理和日志
3. **回滚机制**：提供迁移失败的回滚方案

## 成功标准

1. ✅ DXF文件能够正确转换和预处理
2. ✅ 与control-core的调用接口兼容
3. ✅ 性能表现符合预期
4. ✅ 错误处理机制完善
5. ✅ 文档和测试覆盖完整

## 下一步行动

1. 在dxf-pipeline项目中创建上述文件结构
2. 实现具体的转换逻辑（复用现有dxf_to_pb.py代码）
3. 测试迁移后的功能完整性
4. 更新control-core中的调用路径

这个计划提供了完整的迁移实施方案，确保DXF预处理功能能够平滑迁移到dxf-pipeline项目。