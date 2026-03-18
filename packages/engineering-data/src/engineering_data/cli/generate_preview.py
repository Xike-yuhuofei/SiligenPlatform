from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Sequence

from engineering_data.contracts.preview import PreviewRequest
from engineering_data.preview.html_preview import generate_preview


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Generate a local HTML preview for a DXF file."
    )
    parser.add_argument("--input", required=True, help="Input DXF file")
    parser.add_argument("--output-dir", required=True, help="Directory for generated HTML preview")
    parser.add_argument("--title", default="", help="Optional preview title")
    parser.add_argument("--speed", type=float, default=10.0, help="Estimated speed in mm/s")
    parser.add_argument("--curve-tolerance-mm", type=float, default=0.25,
                        help="Polyline flattening tolerance in mm")
    parser.add_argument("--max-points", type=int, default=12000,
                        help="Maximum sampled points kept in the preview")
    parser.add_argument("--json", action="store_true", help="Print preview metadata as JSON")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    input_path = Path(args.input)
    output_dir = Path(args.output_dir)
    title = args.title.strip() or f"DXF 预览 - {input_path.name}"

    try:
        artifact = generate_preview(
            PreviewRequest(
                input_path=input_path,
                output_dir=output_dir,
                title=title,
                speed_mm_s=float(args.speed),
                curve_tolerance_mm=float(args.curve_tolerance_mm),
                max_points=int(args.max_points),
            )
        )
    except Exception as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 2

    if args.json:
        print(json.dumps(artifact.to_dict(), ensure_ascii=False))
    else:
        print(f"Generated preview -> {artifact.preview_path}")
        print(f"Entities: {artifact.entity_count}")
        print(f"Segments: {artifact.segment_count}")
        print(f"Total length: {artifact.total_length_mm:.2f} mm")
        print(f"Estimated time: {artifact.estimated_time_s:.2f} s")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
