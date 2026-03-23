# Engineering Data CLI Check

- generated_at: `2026-03-23T10:43:39.2128134+08:00`
- python_exe: `python`
- require_console_scripts: `False`
- required_checks_passed: `True`
- console_scripts_passed: `True`
- overall_status: `passed`

## Checks

| Label | Kind | Exit Code | Passed |
|---|---|---|---|
| import engineering_data | required | 0 | True |
| module dxf_to_pb | required | 0 | True |
| module path_to_trajectory | required | 0 | True |
| module export_simulation_input | required | 0 | True |
| module generate_preview | required | 0 | True |
| console-script engineering-data-dxf-to-pb | console_script | 0 | True |
| console-script engineering-data-path-to-trajectory | console_script | 0 | True |
| console-script engineering-data-export-simulation-input | console_script | 0 | True |
| console-script engineering-data-generate-preview | console_script | 0 | True |

## import engineering_data

- command: `python -c import engineering_data`
- exit_code: `0`
- passed: `True`

```text
<empty>
```

## module dxf_to_pb

- command: `python -m engineering_data.cli.dxf_to_pb --help`
- exit_code: `0`
- passed: `True`

```text
usage: dxf_to_pb.py [-h] --input INPUT --output OUTPUT
                    [--schema-version SCHEMA_VERSION] [--chordal CHORDAL]
                    [--max-seg MAX_SEG] [--snap SNAP] [--angular ANGULAR]
                    [--min-seg MIN_SEG]
                    [--normalize-units | --no-normalize-units]
                    [--approx-splines | --no-approx-splines]
                    [--snap-enabled | --no-snap-enabled]
                    [--densify-enabled | --no-densify-enabled]
                    [--min-seg-enabled | --no-min-seg-enabled]
                    [--spline-samples SPLINE_SAMPLES]
                    [--spline-max-step SPLINE_MAX_STEP]
                    [--strict-r12 | --no-strict-r12]

DXF -> Protobuf (.pb) export

options:
  -h, --help            show this help message and exit
  --input INPUT         Input DXF file path
  --output OUTPUT       Output .pb file path
  --schema-version SCHEMA_VERSION
  --chordal CHORDAL
  --max-seg MAX_SEG
  --snap SNAP
  --angular ANGULAR
  --min-seg MIN_SEG
  --normalize-units, --no-normalize-units
  --approx-splines, --no-approx-splines
  --snap-enabled, --no-snap-enabled
  --densify-enabled, --no-densify-enabled
  --min-seg-enabled, --no-min-seg-enabled
  --spline-samples SPLINE_SAMPLES
  --spline-max-step SPLINE_MAX_STEP
  --strict-r12, --no-strict-r12
                        Reject non-R12 DXF versions and compatibility-only
                        entities
```

## module path_to_trajectory

- command: `python -m engineering_data.cli.path_to_trajectory --help`
- exit_code: `0`
- passed: `True`

```text
usage: path_to_trajectory.py [-h] --input INPUT --output OUTPUT --vmax VMAX
                             --amax AMAX --jmax JMAX [--sample-dt SAMPLE_DT]
                             [--sample-ds SAMPLE_DS]

Generate trajectory from polyline points using Ruckig

options:
  -h, --help            show this help message and exit
  --input INPUT         Input path points JSON
  --output OUTPUT       Output trajectory JSON
  --vmax VMAX
  --amax AMAX
  --jmax JMAX
  --sample-dt SAMPLE_DT
  --sample-ds SAMPLE_DS
```

## module export_simulation_input

- command: `python -m engineering_data.cli.export_simulation_input --help`
- exit_code: `0`
- passed: `True`

```text
usage: export_simulation_input.py [-h] --input INPUT --output OUTPUT
                                  [--trigger-points-csv TRIGGER_POINTS_CSV]
                                  [--trigger-channel TRIGGER_CHANNEL]
                                  [--trigger-state | --no-trigger-state]
                                  [--ellipse-max-seg ELLIPSE_MAX_SEG]
                                  [--timestep-seconds TIMESTEP_SECONDS]
                                  [--max-simulation-time-seconds MAX_SIMULATION_TIME_SECONDS]
                                  [--max-velocity MAX_VELOCITY]
                                  [--max-acceleration MAX_ACCELERATION]
                                  [--valve-open-delay-seconds VALVE_OPEN_DELAY_SECONDS]
                                  [--valve-close-delay-seconds VALVE_CLOSE_DELAY_SECONDS]

Export packages/simulation-engine compatible JSON from a PathBundle (.pb).

options:
  -h, --help            show this help message and exit
  --input INPUT         Input PathBundle .pb file
  --output OUTPUT       Output *.simulation-input.json file
  --trigger-points-csv TRIGGER_POINTS_CSV
                        Optional CSV with x_mm,y_mm and optional channel,state
                        columns
  --trigger-channel TRIGGER_CHANNEL
                        Default channel for trigger CSV rows
  --trigger-state, --no-trigger-state
                        Default state for trigger CSV rows without a state
                        column
  --ellipse-max-seg ELLIPSE_MAX_SEG
                        Linearization step in mm for ellipse primitives
  --timestep-seconds TIMESTEP_SECONDS
  --max-simulation-time-seconds MAX_SIMULATION_TIME_SECONDS
  --max-velocity MAX_VELOCITY
  --max-acceleration MAX_ACCELERATION
  --valve-open-delay-seconds VALVE_OPEN_DELAY_SECONDS
  --valve-close-delay-seconds VALVE_CLOSE_DELAY_SECONDS
```

## module generate_preview

- command: `python -m engineering_data.cli.generate_preview --help`
- exit_code: `0`
- passed: `True`

```text
usage: generate_preview.py [-h] --input INPUT --output-dir OUTPUT_DIR
                           [--title TITLE] [--speed SPEED]
                           [--curve-tolerance-mm CURVE_TOLERANCE_MM]
                           [--max-points MAX_POINTS] [--deterministic]
                           [--json]

Generate a local HTML preview for a DXF file.

options:
  -h, --help            show this help message and exit
  --input INPUT         Input DXF file
  --output-dir OUTPUT_DIR
                        Directory for generated HTML preview
  --title TITLE         Optional preview title
  --speed SPEED         Estimated speed in mm/s
  --curve-tolerance-mm CURVE_TOLERANCE_MM
                        Polyline flattening tolerance in mm
  --max-points MAX_POINTS
                        Maximum sampled points kept in the preview
  --deterministic       Use deterministic output filename
                        (<stem>-preview.html)
  --json                Print preview metadata as JSON
```

## console-script engineering-data-dxf-to-pb

- command: `where.exe engineering-data-dxf-to-pb`
- exit_code: `0`
- passed: `True`

```text
C:\Program Files\Python312\Scripts\engineering-data-dxf-to-pb.exe
```

## console-script engineering-data-path-to-trajectory

- command: `where.exe engineering-data-path-to-trajectory`
- exit_code: `0`
- passed: `True`

```text
C:\Program Files\Python312\Scripts\engineering-data-path-to-trajectory.exe
```

## console-script engineering-data-export-simulation-input

- command: `where.exe engineering-data-export-simulation-input`
- exit_code: `0`
- passed: `True`

```text
C:\Program Files\Python312\Scripts\engineering-data-export-simulation-input.exe
```

## console-script engineering-data-generate-preview

- command: `where.exe engineering-data-generate-preview`
- exit_code: `0`
- passed: `True`

```text
C:\Program Files\Python312\Scripts\engineering-data-generate-preview.exe
```
