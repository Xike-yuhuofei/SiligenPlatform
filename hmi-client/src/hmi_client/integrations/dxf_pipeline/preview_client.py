from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class DxfPreviewResult:
    ok: bool
    preview_path: str = ""
    entity_count: int = 0
    segment_count: int = 0
    point_count: int = 0
    total_length_mm: float = 0.0
    estimated_time_s: float = 0.0
    error_message: str = ""


class DxfPipelinePreviewClient:
    """Invoke the sibling dxf-pipeline project to generate local DXF previews."""

    def __init__(self, siligen_root: Path | None = None):
        self._siligen_root = siligen_root or Path(__file__).resolve().parents[5]
        self._pipeline_root = self._siligen_root / "dxf-pipeline"
        self._pipeline_src = self._pipeline_root / "src"
        self._output_dir = Path(tempfile.gettempdir()) / "siligen-suite" / "dxf-preview"

    def generate_preview(
        self,
        dxf_path: str,
        speed_mm_s: float,
        title: str = "",
        timeout_s: float = 60.0,
        max_points: int = 12000,
    ) -> DxfPreviewResult:
        input_path = Path(dxf_path)
        if not input_path.exists():
            return DxfPreviewResult(ok=False, error_message=f"DXF 文件不存在: {input_path}")
        if not self._pipeline_src.exists():
            return DxfPreviewResult(ok=False, error_message=f"dxf-pipeline 源码目录不存在: {self._pipeline_src}")

        python_executable = self._resolve_python_executable()
        command = [
            str(python_executable),
            "-m",
            "dxf_pipeline.cli.generate_preview",
            "--input",
            str(input_path),
            "--output-dir",
            str(self._output_dir),
            "--speed",
            f"{float(speed_mm_s):.6f}",
            "--max-points",
            str(int(max_points)),
            "--json",
        ]
        if title:
            command.extend(["--title", title])

        env = os.environ.copy()
        existing_python_path = env.get("PYTHONPATH", "")
        python_path_entries = [str(self._pipeline_src)]
        if existing_python_path:
            python_path_entries.append(existing_python_path)
        env["PYTHONPATH"] = os.pathsep.join(python_path_entries)

        try:
            completed = subprocess.run(
                command,
                check=False,
                capture_output=True,
                text=True,
                encoding="utf-8",
                timeout=timeout_s,
                env=env,
            )
        except subprocess.TimeoutExpired:
            return DxfPreviewResult(ok=False, error_message="DXF 预览生成超时")
        except Exception as exc:
            return DxfPreviewResult(ok=False, error_message=f"启动 dxf-pipeline 失败: {exc}")

        stdout_text = (completed.stdout or "").strip()
        stderr_text = (completed.stderr or "").strip()
        if completed.returncode != 0:
            error_message = stderr_text or stdout_text or f"dxf-pipeline 退出码异常: {completed.returncode}"
            return DxfPreviewResult(ok=False, error_message=error_message)

        try:
            payload = json.loads(stdout_text.splitlines()[-1])
        except Exception as exc:
            return DxfPreviewResult(ok=False, error_message=f"解析 dxf-pipeline 输出失败: {exc}")

        preview_path = str(payload.get("preview_path", ""))
        if not preview_path:
            return DxfPreviewResult(ok=False, error_message="dxf-pipeline 未返回 preview_path")

        return DxfPreviewResult(
            ok=True,
            preview_path=preview_path,
            entity_count=int(payload.get("entity_count", 0)),
            segment_count=int(payload.get("segment_count", 0)),
            point_count=int(payload.get("point_count", 0)),
            total_length_mm=float(payload.get("total_length_mm", 0.0)),
            estimated_time_s=float(payload.get("estimated_time_s", 0.0)),
        )

    def _resolve_python_executable(self) -> Path:
        override = os.getenv("SILIGEN_DXF_PIPELINE_PYTHON", "").strip()
        if override:
            return Path(override)

        venv_python = self._pipeline_root / ".venv" / "Scripts" / "python.exe"
        if venv_python.exists():
            return venv_python

        return Path(sys.executable)
