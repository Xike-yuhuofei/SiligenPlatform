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