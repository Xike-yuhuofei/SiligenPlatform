import argparse
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path


def find_project_root(start: Path) -> Path:
    current = start
    for _ in range(10):
        if (current / "pyproject.toml").exists() and (current / "PROJECT_BOUNDARY_RULES.md").exists():
            return current
        if current.parent == current:
            break
        current = current.parent
    return start.parent if start.is_file() else start


def load_config(path: Path) -> dict:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise ValueError("config root must be a JSON object")
    return data


def resolve_path(value: str, repo_root: Path) -> Path:
    path = Path(value)
    if path.is_absolute():
        return path
    return (repo_root / path).resolve()


def slugify(text: str) -> str:
    out = []
    for ch in text:
        if "a" <= ch <= "z" or "A" <= ch <= "Z" or "0" <= ch <= "9":
            out.append(ch)
        else:
            out.append("_")
    slug = "".join(out).strip("_")
    return slug or "case"


def build_cases(config: dict) -> list:
    base_args = config.get("base_args", []) or []
    include_base = bool(config.get("include_base", True))
    switches = config.get("switches", []) or []
    explicit_cases = config.get("cases", []) or []

    cases = []
    seen = set()

    def add_case(case_id: str, label: str, args: list, switch_key: str, state: str):
        key = tuple(args)
        if key in seen:
            return
        seen.add(key)
        cases.append({
            "id": case_id,
            "label": label,
            "args": args,
            "switch": switch_key,
            "state": state,
        })

    if include_base:
        add_case("base", "Base", base_args, "base", "base")

    for case in explicit_cases:
        name = str(case.get("name", "case"))
        label = str(case.get("label", name))
        args = base_args + list(case.get("args", []))
        add_case(name, label, args, "custom", "custom")

    for sw in switches:
        key = str(sw.get("key", "switch"))
        label = str(sw.get("label", key))
        off_args = base_args + list(sw.get("off_args", []))
        on_args = base_args + list(sw.get("on_args", []))
        add_case(f"{key}_off", f"{label} [off]", off_args, key, "off")
        add_case(f"{key}_on", f"{label} [on]", on_args, key, "on")

    return cases


def compute_preview_path(dxf_path: Path) -> Path:
    stem = dxf_path.stem or "dxf_preview"
    return dxf_path.with_name(f"{stem}_preview.html")


def wait_for_file(path: Path, min_mtime: float, timeout_s: float = 5.0) -> bool:
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        if path.exists():
            try:
                if path.stat().st_mtime >= min_mtime:
                    return True
            except OSError:
                pass
        time.sleep(0.1)
    return path.exists()


def run_case(cli_path: Path, working_dir: Path, dxf_path: Path, global_args: list, case: dict, output_dir: Path):
    cmd = [str(cli_path), "dxf-plan", "--no-interactive", "--file", str(dxf_path)]
    cmd.extend(global_args)
    cmd.extend(case["args"])

    preview_src = compute_preview_path(dxf_path)
    before_mtime = preview_src.stat().st_mtime if preview_src.exists() else 0.0

    start_time = time.time()
    proc = subprocess.run(
        cmd,
        cwd=str(working_dir),
        capture_output=True,
        text=True,
    )

    success = proc.returncode == 0
    updated = wait_for_file(preview_src, max(before_mtime, start_time))

    logs_dir = output_dir / "logs"
    logs_dir.mkdir(parents=True, exist_ok=True)
    (logs_dir / f"{case['id']}.stdout.txt").write_text(proc.stdout or "", encoding="utf-8")
    (logs_dir / f"{case['id']}.stderr.txt").write_text(proc.stderr or "", encoding="utf-8")

    output_rel = ""
    error = ""
    if not success:
        error = f"cli failed with code {proc.returncode}"
    elif not updated:
        error = "preview html not generated"
    else:
        slug = slugify(case["id"])
        output_name = f"{slug}.html"
        dest = output_dir / output_name
        try:
            shutil.copyfile(preview_src, dest)
            output_rel = output_name
        except OSError as exc:
            error = f"copy failed: {exc}"

    return {
        "id": case["id"],
        "label": case["label"],
        "switch": case["switch"],
        "state": case["state"],
        "args": case["args"],
        "cmd": cmd,
        "returncode": proc.returncode,
        "success": success and not error,
        "error": error,
        "output": output_rel,
    }


def write_index(output_dir: Path, dxf_path: Path, results: list):
    manifest = {
        "dxf_path": str(dxf_path),
        "results": results,
    }
    (output_dir / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=True, indent=2),
        encoding="utf-8",
    )

    cases_json = json.dumps(results, ensure_ascii=True)

    rows = []
    for idx, item in enumerate(results, 1):
        link = ""
        if item["output"]:
            link = f"<a href=\"{item['output']}\">open</a>"
        status = "ok" if item["success"] else "fail"
        args = " ".join(item["args"])
        rows.append(
            "<tr>"
            f"<td>{idx}</td>"
            f"<td>{item['label']}</td>"
            f"<td>{item['switch']}</td>"
            f"<td>{item['state']}</td>"
            f"<td class=\"{status}\">{status}</td>"
            f"<td>{link}</td>"
            f"<td class=\"mono\">{args}</td>"
            "</tr>"
        )

    html = """<!doctype html>
<html>
<head>
<meta charset=\"utf-8\">
<title>Trajectory Compare</title>
<style>
body { font-family: Arial, sans-serif; margin: 20px; }
header { margin-bottom: 16px; }
table { border-collapse: collapse; width: 100%; }
th, td { border: 1px solid #ccc; padding: 6px 8px; font-size: 13px; }
th { background: #f2f2f2; text-align: left; }
.ok { color: #1a7f37; font-weight: bold; }
.fail { color: #c20000; font-weight: bold; }
.mono { font-family: Consolas, Menlo, monospace; font-size: 12px; }
.compare { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin: 12px 0 20px; }
.compare iframe { width: 100%; height: 520px; border: 1px solid #ccc; }
.controls { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-bottom: 8px; }
select { width: 100%; padding: 6px; }
</style>
</head>
<body>
<header>
  <h1>Trajectory Compare</h1>
  <div>DXF: """ + str(dxf_path).replace("&", "&amp;") + """</div>
</header>
<section>
  <div class=\"controls\">
    <select id=\"leftSelect\"></select>
    <select id=\"rightSelect\"></select>
  </div>
  <div class=\"compare\">
    <iframe id=\"leftFrame\"></iframe>
    <iframe id=\"rightFrame\"></iframe>
  </div>
</section>
<table>
  <thead>
    <tr>
      <th>#</th>
      <th>Case</th>
      <th>Switch</th>
      <th>State</th>
      <th>Status</th>
      <th>Link</th>
      <th>Args</th>
    </tr>
  </thead>
  <tbody>
""" + "\n".join(rows) + """
  </tbody>
</table>
<script>
const cases = """ + cases_json + """;
const okCases = cases.filter(c => c.output);
const leftSelect = document.getElementById('leftSelect');
const rightSelect = document.getElementById('rightSelect');
const leftFrame = document.getElementById('leftFrame');
const rightFrame = document.getElementById('rightFrame');

function fillSelect(select) {
  select.innerHTML = '';
  okCases.forEach((c, idx) => {
    const opt = document.createElement('option');
    opt.value = c.output;
    opt.textContent = c.label;
    select.appendChild(opt);
  });
}

function bind(select, frame) {
  select.addEventListener('change', () => {
    frame.src = select.value || '';
  });
}

fillSelect(leftSelect);
fillSelect(rightSelect);
if (okCases.length > 0) {
  leftSelect.selectedIndex = 0;
  rightSelect.selectedIndex = Math.min(1, okCases.length - 1);
  leftFrame.src = leftSelect.value;
  rightFrame.src = rightSelect.value;
}

bind(leftSelect, leftFrame);
bind(rightSelect, rightFrame);
</script>
</body>
</html>
"""

    (output_dir / "index.html").write_text(html, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Run trajectory preview comparisons")
    parser.add_argument("--config", default="benchmarks/trajectory_compare/compare_config.json")
    parser.add_argument("--cli-path", default="")
    parser.add_argument("--dxf-path", default="")
    parser.add_argument("--output-dir", default="")
    args = parser.parse_args()

    project_root = find_project_root(Path(__file__).resolve())
    config_path = resolve_path(args.config, project_root)
    if not config_path.exists():
        print(f"config not found: {config_path}")
        return 1

    config = load_config(config_path)

    if args.cli_path:
        config["cli_path"] = args.cli_path
    if args.dxf_path:
        config["dxf_path"] = args.dxf_path
    if args.output_dir:
        config["output_dir"] = args.output_dir

    cli_path = resolve_path(config.get("cli_path", ""), project_root)
    dxf_path = resolve_path(config.get("dxf_path", ""), project_root)
    output_root = resolve_path(config.get("output_dir", "benchmarks/reports/trajectory_compare"), project_root)

    if not cli_path.exists():
        print(f"cli not found: {cli_path}")
        return 1
    if not dxf_path.exists():
        print(f"dxf not found: {dxf_path}")
        return 1

    working_dir = resolve_path(config.get("working_dir", "../control-core"), project_root)
    if not working_dir.exists():
        print(f"working_dir not found: {working_dir}")
        return 1

    timestamp = time.strftime("%Y%m%d_%H%M%S")
    output_dir = output_root / f"run_{timestamp}"
    output_dir.mkdir(parents=True, exist_ok=True)

    global_args = list(config.get("global_args", []) or [])
    cases = build_cases(config)

    results = []
    for case in cases:
        result = run_case(cli_path, working_dir, dxf_path, global_args, case, output_dir)
        results.append(result)

    write_index(output_dir, dxf_path, results)
    print(f"done: {output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
