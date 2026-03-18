from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Sequence

from engineering_data.contracts.simulation_input import (
    DEFAULT_EXPORTS,
    ExportDefaults,
    ValveConfig,
    bundle_to_simulation_payload,
    collect_export_notes,
    load_path_bundle,
    load_trigger_points_csv,
    write_simulation_payload,
)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Export packages/simulation-engine compatible JSON from a PathBundle (.pb)."
    )
    parser.add_argument("--input", required=True, help="Input PathBundle .pb file")
    parser.add_argument("--output", required=True, help="Output *.simulation-input.json file")
    parser.add_argument("--trigger-points-csv", help="Optional CSV with x_mm,y_mm and optional channel,state columns")
    parser.add_argument("--trigger-channel", default="DO_CAMERA_TRIGGER", help="Default channel for trigger CSV rows")
    parser.add_argument("--trigger-state", action=argparse.BooleanOptionalAction, default=True,
                        help="Default state for trigger CSV rows without a state column")
    parser.add_argument("--ellipse-max-seg", type=float, default=1.0,
                        help="Linearization step in mm for ellipse primitives")
    parser.add_argument("--timestep-seconds", type=float, default=DEFAULT_EXPORTS.timestep_seconds)
    parser.add_argument("--max-simulation-time-seconds", type=float, default=DEFAULT_EXPORTS.max_simulation_time_seconds)
    parser.add_argument("--max-velocity", type=float, default=DEFAULT_EXPORTS.max_velocity)
    parser.add_argument("--max-acceleration", type=float, default=DEFAULT_EXPORTS.max_acceleration)
    parser.add_argument("--valve-open-delay-seconds", type=float, default=DEFAULT_EXPORTS.valve.open_delay_seconds)
    parser.add_argument("--valve-close-delay-seconds", type=float, default=DEFAULT_EXPORTS.valve.close_delay_seconds)
    return parser


def _defaults_from_args(args: argparse.Namespace) -> ExportDefaults:
    return ExportDefaults(
        timestep_seconds=float(args.timestep_seconds),
        max_simulation_time_seconds=float(args.max_simulation_time_seconds),
        max_velocity=float(args.max_velocity),
        max_acceleration=float(args.max_acceleration),
        io_delays=DEFAULT_EXPORTS.io_delays,
        valve=ValveConfig(
            open_delay_seconds=float(args.valve_open_delay_seconds),
            close_delay_seconds=float(args.valve_close_delay_seconds),
        ),
    )


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    input_path = Path(args.input)
    output_path = Path(args.output)
    if not input_path.exists():
        print(f"Error: input PathBundle not found: {input_path}", file=sys.stderr)
        return 2

    bundle = load_path_bundle(input_path)
    trigger_points = []
    if args.trigger_points_csv:
        trigger_points = load_trigger_points_csv(
            path=Path(args.trigger_points_csv),
            default_channel=str(args.trigger_channel),
            default_state=bool(args.trigger_state),
        )

    payload = bundle_to_simulation_payload(
        bundle=bundle,
        defaults=_defaults_from_args(args),
        trigger_points=trigger_points,
        ellipse_max_seg=float(args.ellipse_max_seg),
    )
    write_simulation_payload(output_path, payload)

    print(f"Exported simulation input -> {output_path}")
    for note in collect_export_notes(bundle):
        print(f"Note: {note}")
    if args.trigger_points_csv:
        print(f"Mapped {len(payload['triggers'])} trigger points -> cumulative path positions")
    else:
        print("Triggers exported as [] (no trigger source was provided)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
