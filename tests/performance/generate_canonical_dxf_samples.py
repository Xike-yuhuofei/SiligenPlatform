from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_OUTPUT_DIR = ROOT / "samples" / "dxf"


@dataclass(frozen=True)
class SampleSpec:
    file_name: str
    cols: int
    rows: int
    pitch_mm: float
    description: str

    @property
    def width_mm(self) -> float:
        return round((self.cols - 1) * self.pitch_mm, 3)

    @property
    def height_mm(self) -> float:
        return round((self.rows - 1) * self.pitch_mm, 3)

    @property
    def segment_count(self) -> int:
        return self.rows * max(self.cols - 1, 0) + max(self.rows - 1, 0)


SAMPLE_SPECS: dict[str, SampleSpec] = {
    "medium": SampleSpec(
        file_name="rect_medium_ladder.dxf",
        cols=24,
        rows=16,
        pitch_mm=5.0,
        description="line-only serpentine DXF used as the canonical medium nightly-performance sample",
    ),
    "large": SampleSpec(
        file_name="rect_large_ladder.dxf",
        cols=40,
        rows=48,
        pitch_mm=4.0,
        description="line-only serpentine DXF used as the canonical large nightly-performance sample",
    ),
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate canonical medium/large DXF samples for nightly-performance.")
    parser.add_argument(
        "--sample-labels",
        nargs="*",
        choices=sorted(SAMPLE_SPECS),
        default=sorted(SAMPLE_SPECS),
        help="Which canonical samples to generate.",
    )
    parser.add_argument("--output-dir", default=str(DEFAULT_OUTPUT_DIR))
    return parser.parse_args()


def _fmt(value: float) -> str:
    if value.is_integer():
        return f"{value:.1f}"
    return f"{value:.3f}".rstrip("0").rstrip(".")


def _build_serpentine_points(spec: SampleSpec) -> list[tuple[float, float]]:
    points: list[tuple[float, float]] = []
    for row in range(spec.rows):
        y = row * spec.pitch_mm
        if row % 2 == 0:
            xs = [column * spec.pitch_mm for column in range(spec.cols)]
        else:
            xs = [column * spec.pitch_mm for column in reversed(range(spec.cols))]
        row_points = [(x, y) for x in xs]
        if not points:
            points.extend(row_points)
            continue
        points.extend(row_points)
    return points


def _render_dxf(spec: SampleSpec) -> str:
    points = _build_serpentine_points(spec)
    if len(points) < 2:
        raise ValueError(f"{spec.file_name}: serpentine path requires at least 2 points")

    lines = [
        "0",
        "SECTION",
        "2",
        "HEADER",
        "9",
        "$ACADVER",
        "1",
        "AC1015",
        "9",
        "$INSUNITS",
        "70",
        "4",
        "9",
        "$EXTMIN",
        "10",
        "0.0",
        "20",
        "0.0",
        "30",
        "0.0",
        "9",
        "$EXTMAX",
        "10",
        _fmt(spec.width_mm),
        "20",
        _fmt(spec.height_mm),
        "30",
        "0.0",
        "0",
        "ENDSEC",
        "0",
        "SECTION",
        "2",
        "TABLES",
        "0",
        "ENDSEC",
        "0",
        "SECTION",
        "2",
        "ENTITIES",
    ]

    for start, end in zip(points, points[1:]):
        lines.extend(
            [
                "0",
                "LINE",
                "8",
                "0",
                "10",
                _fmt(start[0]),
                "20",
                _fmt(start[1]),
                "30",
                "0.0",
                "11",
                _fmt(end[0]),
                "21",
                _fmt(end[1]),
                "31",
                "0.0",
            ]
        )

    lines.extend(["0", "ENDSEC", "0", "EOF"])
    return "\n".join(lines) + "\n"


def generate_sample(output_dir: Path, label: str) -> Path:
    spec = SAMPLE_SPECS[label]
    output_dir.mkdir(parents=True, exist_ok=True)
    output_path = output_dir / spec.file_name
    output_path.write_text(_render_dxf(spec), encoding="utf-8")
    return output_path


def main() -> int:
    args = parse_args()
    output_dir = Path(args.output_dir).resolve()
    for label in args.sample_labels:
        output_path = generate_sample(output_dir, label)
        spec = SAMPLE_SPECS[label]
        print(
            f"generated {label}: {output_path} "
            f"(segments={spec.segment_count} width_mm={_fmt(spec.width_mm)} height_mm={_fmt(spec.height_mm)})"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
