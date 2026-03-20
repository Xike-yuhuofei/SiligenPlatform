#!/usr/bin/env python3
"""dxf_pipeline compatibility service hosted by engineering-data."""

from __future__ import annotations

from dataclasses import dataclass
import time
from typing import Optional

import ezdxf

from engineering_data.processing.dxf_to_pb import main as canonical_dxf_to_pb_main


@dataclass
class PreprocessingResult:
    success: bool
    output_path: str
    error_message: Optional[str] = None
    processing_time_ms: int = 0
    entities_processed: int = 0


@dataclass
class ValidationResult:
    is_valid: bool
    format_version: str
    error_details: Optional[str] = None
    entity_count: int = 0
    requires_preprocessing: bool = False


@dataclass
class PreprocessingConfig:
    chordal_tolerance: float = 0.005
    max_segment_length: float = 0.0
    angular_tolerance: float = 0.0
    min_segment_length: float = 0.0
    output_format: str = "pb"


class DXFPreprocessingService:
    def __init__(self) -> None:
        self.supported_formats = ["R12", "R14", "2000", "2004", "2007", "2010"]

    def convert_to_r12(self, input_path: str, output_path: str) -> PreprocessingResult:
        return self.preprocess_dxf(input_path, PreprocessingConfig(), output_path=output_path)

    def preprocess_dxf(
        self,
        dxf_path: str,
        config: PreprocessingConfig,
        output_path: str | None = None,
    ) -> PreprocessingResult:
        start_time = time.time()
        pb_path = output_path or f"{dxf_path}.pb"
        exit_code = canonical_dxf_to_pb_main([
            "--input",
            dxf_path,
            "--output",
            pb_path,
            "--chordal",
            str(config.chordal_tolerance),
            "--max-seg",
            str(config.max_segment_length),
            "--angular",
            str(config.angular_tolerance),
            "--min-seg",
            str(config.min_segment_length),
        ])
        duration = int((time.time() - start_time) * 1000)
        if exit_code != 0:
            return PreprocessingResult(
                success=False,
                output_path="",
                error_message=f"canonical dxf_to_pb failed with exit code {exit_code}",
                processing_time_ms=duration,
            )

        entity_count = 0
        try:
            entity_count = len(ezdxf.readfile(dxf_path).modelspace())
        except Exception:
            entity_count = 0

        return PreprocessingResult(
            success=True,
            output_path=pb_path,
            processing_time_ms=duration,
            entities_processed=entity_count,
        )

    def validate_dxf(self, file_path: str) -> ValidationResult:
        try:
            doc = ezdxf.readfile(file_path)
            return ValidationResult(
                is_valid=True,
                format_version=str(doc.dxfversion),
                entity_count=len(doc.modelspace()),
                requires_preprocessing=not str(doc.dxfversion).upper().startswith("AC1009"),
            )
        except Exception as exc:
            return ValidationResult(
                is_valid=False,
                format_version="Unknown",
                error_details=str(exc),
            )

    def get_supported_formats(self) -> list[str]:
        return self.supported_formats


def main() -> None:
    import sys

    if len(sys.argv) < 3:
        raise SystemExit("用法: python dxf_preprocessing.py <input.dxf> <output.pb>")

    service = DXFPreprocessingService()
    result = service.preprocess_dxf(sys.argv[1], PreprocessingConfig(), output_path=sys.argv[2])
    if result.success:
        print(f"预处理成功: {result.output_path}")
        print(f"处理时间: {result.processing_time_ms}ms")
        print(f"处理实体: {result.entities_processed}个")
        return
    raise SystemExit(result.error_message or "预处理失败")
