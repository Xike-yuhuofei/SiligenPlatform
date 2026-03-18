# DXF-Pipeline 迁移实施指导

## 给AI的提示词

请立即在 `D:\Projects\SiligenSuite\dxf-pipeline` 项目中创建以下文件结构：

### 第一步：创建服务模块结构

**1. 创建服务目录**：
```bash
mkdir -p src/dxf_pipeline/services
```

**2. 创建服务初始化文件**：

**文件**: `src/dxf_pipeline/services/__init__.py`
```python
# DXF Pipeline 服务模块
"""
DXF Pipeline 服务模块

提供统一的DXF预处理服务接口，支持从control-core迁移的功能。
"""

from .dxf_preprocessing import DXFPreprocessingService, PreprocessingResult, ValidationResult, PreprocessingConfig
from .migration_adapter import MigrationAdapter

__all__ = [
    'DXFPreprocessingService',
    'PreprocessingResult', 
    'ValidationResult',
    'PreprocessingConfig',
    'MigrationAdapter'
]
```

**3. 创建预处理服务接口**：

**文件**: `src/dxf_pipeline/services/dxf_preprocessing.py`
```python
#!/usr/bin/env python3
"""DXF预处理服务 - 统一处理DXF文件转换和预处理"""

import os
import sys
import time
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
        start_time = time.time()
        
        try:
            # 复用现有的dxf_to_pb.py功能
            from ..cli.dxf_to_pb import main as dxf_to_pb_main
            
            # 创建临时参数列表
            import sys
            original_argv = sys.argv.copy()
            sys.argv = ['dxf_to_pb.py', dxf_path, dxf_path + '.pb']
            
            # 执行转换
            dxf_to_pb_main()
            
            processing_time = int((time.time() - start_time) * 1000)
            return PreprocessingResult(
                success=True,
                output_path=dxf_path + '.pb',
                processing_time_ms=processing_time,
                entities_processed=100  # 示例值，实际需要统计
            )
            
        except Exception as e:
            processing_time = int((time.time() - start_time) * 1000)
            return PreprocessingResult(
                success=False,
                output_path="",
                error_message=str(e),
                processing_time_ms=processing_time
            )
        finally:
            # 恢复原始参数
            sys.argv = original_argv
    
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

def main():
    """命令行入口点"""
    if len(sys.argv) < 3:
        print("用法: python dxf_preprocessing.py <input.dxf> <output.pb>")
        sys.exit(1)
    
    service = DXFPreprocessingService()
    result = service.preprocess_dxf(sys.argv[1], PreprocessingConfig())
    
    if result.success:
        print(f"预处理成功: {result.output_path}")
        print(f"处理时间: {result.processing_time_ms}ms")
        print(f"处理实体: {result.entities_processed}个")
    else:
        print(f"预处理失败: {result.error_message}")
        sys.exit(1)

if __name__ == "__main__":
    main()
```

**4. 创建迁移适配器**：

**文件**: `src/dxf_pipeline/services/migration_adapter.py`
```python
#!/usr/bin/env python3
"""迁移适配器 - 提供与control-core兼容的接口"""

import json
import sys
from typing import Dict, Any
from .dxf_preprocessing import DXFPreprocessingService

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

def main():
    """命令行入口点"""
    adapter = MigrationAdapter()
    response = adapter.handle_command_line_request(sys.argv)
    
    # 输出JSON格式结果，便于程序调用
    print(json.dumps(response, indent=2))

if __name__ == "__main__":
    main()
```

### 第二步：创建命令行工具

**5. 创建迁移脚本**：

**文件**: `scripts/convert_dxf_to_r12.py`
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

**6. 创建测试脚本**：

**文件**: `scripts/test_migration.py`
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
    
    # 测试验证功能（使用示例文件）
    test_file = os.path.join(os.path.dirname(__file__), '..', 'tests', 'fixtures', 'imported', 'uploads-dxf', 'archive', 'Demo.dxf')
    
    if os.path.exists(test_file):
        validation = service.validate_dxf(test_file)
        print(f"文件验证: {'有效' if validation.is_valid else '无效'}")
        print(f"格式版本: {validation.format_version}")
        print(f"实体数量: {validation.entity_count}")
    else:
        print("测试文件不存在，跳过文件验证")

if __name__ == "__main__":
    test_basic_functionality()
    print("基本功能测试完成")
```

### 第三步：测试实施结果

**7. 执行测试**：
```bash
# 在 dxf-pipeline 项目中执行
python scripts/test_migration.py

# 测试转换功能
python scripts/convert_dxf_to_r12.py tests/fixtures/imported/uploads-dxf/archive/Demo.dxf output.pb
```

**重要提示**：请确保在创建文件后验证依赖项是否已安装：
```bash
pip install ezdxf protobuf
```

## 文件结构总结

需要创建的文件：
- `src/dxf_pipeline/services/__init__.py`
- `src/dxf_pipeline/services/dxf_preprocessing.py`
- `src/dxf_pipeline/services/migration_adapter.py`
- `scripts/convert_dxf_to_r12.py`
- `scripts/test_migration.py`

## 实施步骤

1. 切换到 `dxf-pipeline` 项目目录
2. 创建服务模块目录结构
3. 逐一创建上述文件
4. 安装必要的依赖
5. 执行测试验证功能

## 注意事项

- 确保 `ezdxf` 和 `protobuf` 已正确安装
- 验证与现有 `dxf_to_pb.py` 的兼容性
- 测试各种DXF文件格式支持
- 验证错误处理机制