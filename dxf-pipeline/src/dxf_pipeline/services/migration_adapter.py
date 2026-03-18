#!/usr/bin/env python3
"""Compatibility adapter that forwards legacy requests to engineering-data."""

from __future__ import annotations

import json
import sys
from typing import Any

from dxf_pipeline.services.dxf_preprocessing import DXFPreprocessingService


class MigrationAdapter:
    def __init__(self) -> None:
        self.service = DXFPreprocessingService()

    def handle_command_line_request(self, args: list[str]) -> dict[str, Any]:
        if len(args) < 3:
            return self._create_error_response("参数不足，需要输入文件和输出文件路径")

        input_path = args[1]
        output_path = args[2]
        validation = self.service.validate_dxf(input_path)
        if not validation.is_valid:
            return self._create_error_response(f"文件验证失败: {validation.error_details}")

        result = self.service.convert_to_r12(input_path, output_path)
        if not result.success:
            return self._create_error_response(result.error_message or "预处理失败")

        return self._create_success_response({
            "output_path": result.output_path,
            "processing_time": result.processing_time_ms,
            "entities_processed": result.entities_processed,
        })

    def _create_success_response(self, data: dict[str, Any]) -> dict[str, Any]:
        return {
            "status": "success",
            "data": data,
        }

    def _create_error_response(self, message: str) -> dict[str, Any]:
        return {
            "status": "error",
            "error": message,
        }


def main() -> None:
    adapter = MigrationAdapter()
    print(json.dumps(adapter.handle_command_line_request(sys.argv), indent=2))


if __name__ == "__main__":
    main()
