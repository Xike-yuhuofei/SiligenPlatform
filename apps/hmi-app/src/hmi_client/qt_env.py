"""Qt runtime environment bootstrap helpers.

This module must stay free of PyQt imports so it can be called before Qt
libraries are loaded.
"""

from __future__ import annotations

import os
import sys
from importlib.util import find_spec
from pathlib import Path


def configure_qt_environment(*, headless: bool = False) -> None:
    """Configure minimal Qt runtime environment variables.

    - Optionally forces offscreen platform for headless runs.
    - Sets QT_QPA_FONTDIR when possible to avoid noisy font directory warnings.
    """
    if headless:
        os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

    font_dir = _resolve_qt_font_dir()
    if font_dir is not None:
        os.environ.setdefault("QT_QPA_FONTDIR", str(font_dir))


def _resolve_qt_font_dir() -> Path | None:
    configured = os.environ.get("QT_QPA_FONTDIR", "").strip()
    if configured:
        configured_path = Path(configured)
        if configured_path.is_dir():
            return configured_path

    for candidate in _font_dir_candidates():
        if candidate.is_dir():
            return candidate
    return None


def _font_dir_candidates() -> tuple[Path, ...]:
    candidates: list[Path] = []

    spec = find_spec("PyQt5")
    if spec is not None and spec.origin:
        pyqt_root = Path(spec.origin).resolve().parent
        candidates.extend(
            (
                pyqt_root / "Qt5" / "lib" / "fonts",
                pyqt_root / "Qt" / "lib" / "fonts",
                pyqt_root / "Qt5" / "fonts",
            )
        )

    if sys.platform.startswith("win"):
        candidates.append(Path(r"C:\Windows\Fonts"))
    elif sys.platform == "darwin":
        candidates.extend((Path("/System/Library/Fonts"), Path("/Library/Fonts")))
    else:
        candidates.extend((Path("/usr/share/fonts"), Path("/usr/local/share/fonts")))

    deduped: list[Path] = []
    seen: set[str] = set()
    for path in candidates:
        key = str(path)
        if key in seen:
            continue
        seen.add(key)
        deduped.append(path)
    return tuple(deduped)


__all__ = ["configure_qt_environment"]
