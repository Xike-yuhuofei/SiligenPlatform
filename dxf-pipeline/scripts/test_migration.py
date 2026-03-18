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