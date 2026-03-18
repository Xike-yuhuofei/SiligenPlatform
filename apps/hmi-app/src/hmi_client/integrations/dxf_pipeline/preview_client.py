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
    """Preserve the HMI preview integration surface while calling canonical engineering-data."""

    def __init__(self, siligen_root: Path | None = None):
        self._workspace_root = self._resolve_workspace_root(siligen_root)
        self._engineering_data_root = self._resolve_engineering_data_root()
        self._engineering_data_src = self._engineering_data_root / "src"
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
        if not self._engineering_data_src.exists():
            return DxfPreviewResult(ok=False, error_message=f"engineering-data 源码目录不存在: {self._engineering_data_src}")

        python_executable = self._resolve_python_executable()
        command = [
            str(python_executable),
            "-m",
            "engineering_data.cli.generate_preview",
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
        python_path_entries = [str(self._engineering_data_src)]
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
            return DxfPreviewResult(ok=False, error_message=f"启动 engineering-data 失败: {exc}")

        stdout_text = (completed.stdout or "").strip()
        stderr_text = (completed.stderr or "").strip()
        if completed.returncode != 0:
            error_message = stderr_text or stdout_text or f"engineering-data 退出码异常: {completed.returncode}"
            return DxfPreviewResult(ok=False, error_message=error_message)

        try:
            payload = json.loads(stdout_text.splitlines()[-1])
        except Exception as exc:
            return DxfPreviewResult(ok=False, error_message=f"解析 engineering-data 输出失败: {exc}")

        preview_path = str(payload.get("preview_path", ""))
        if not preview_path:
            return DxfPreviewResult(ok=False, error_message="engineering-data 未返回 preview_path")

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
        override = os.getenv("SILIGEN_ENGINEERING_DATA_PYTHON", "").strip()
        if override:
            return Path(override)

        candidates = [
            self._workspace_root / ".venv" / "Scripts" / "python.exe",
            self._engineering_data_root / ".venv" / "Scripts" / "python.exe",
        ]
        for candidate in candidates:
            if candidate.exists():
                return candidate

        return Path(sys.executable)

    def _resolve_workspace_root(self, siligen_root: Path | None) -> Path:
        if siligen_root is not None:
            return Path(siligen_root).expanduser().resolve()

        override = os.getenv("SILIGEN_WORKSPACE_ROOT", "").strip()
        if override:
            return Path(override).expanduser().resolve()

        anchor = Path(__file__).resolve()
        for candidate in anchor.parents:
            if (candidate / "apps").exists() and (candidate / "packages").exists():
                return candidate
        return anchor.parents[0]

    def _resolve_engineering_data_root(self) -> Path:
        override = os.getenv("SILIGEN_ENGINEERING_DATA_ROOT", "").strip()
        if override:
            return Path(override).expanduser().resolve()

        candidate = self._workspace_root / "packages" / "engineering-data"
        if (candidate / "src").exists():
            return candidate

        anchor = Path(__file__).resolve()
        for parent in anchor.parents:
            candidate = parent / "packages" / "engineering-data"
            if (candidate / "src").exists():
                return candidate
        return self._workspace_root / "packages" / "engineering-data"
