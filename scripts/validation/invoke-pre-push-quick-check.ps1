[CmdletBinding()]
param(
    [ValidateSet("changed-file-parse", "protocol-compatibility", "recipe-config-schema", "state-machine-interlock", "offline-simulation-smoke", "validation-system-contract", "contracts-quick")]
    [string]$Check,
    [string]$ReportDir = "tests\reports\pre-push-gate\quick-check",
    [string[]]$ChangedFile = @()
)

$ErrorActionPreference = "Stop"
$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent

function Resolve-WorkspacePath {
    param([string]$PathValue)
    if ([System.IO.Path]::IsPathRooted($PathValue)) { return [System.IO.Path]::GetFullPath($PathValue) }
    return [System.IO.Path]::GetFullPath((Join-Path $workspaceRoot $PathValue))
}

function Expand-CommaSeparatedValues {
    param([string[]]$Values)
    $expanded = @()
    foreach ($value in @($Values)) {
        if ([string]::IsNullOrWhiteSpace($value)) { continue }
        $expanded += @(([string]$value).Split(",") | ForEach-Object { $_.Trim() } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    }
    return @($expanded)
}

function Invoke-Pytest {
    param([string[]]$TestPath)
    $output = & python -m pytest @TestPath -q 2>&1
    $exitCode = if ($null -eq $LASTEXITCODE) { 0 } else { $LASTEXITCODE }
    return [pscustomobject]@{ ExitCode = $exitCode; Output = ($output | Out-String) }
}

function Invoke-PythonScript {
    param([string]$Source, [string[]]$Arguments)
    $scriptPath = Join-Path $reportDirPath "$Check-probe.py"
    Set-Content -Path $scriptPath -Value $Source -Encoding UTF8
    $output = & python $scriptPath @Arguments 2>&1
    $exitCode = if ($null -eq $LASTEXITCODE) { 0 } else { $LASTEXITCODE }
    return [pscustomobject]@{ ExitCode = $exitCode; Output = ($output | Out-String) }
}

$reportDirPath = Resolve-WorkspacePath -PathValue $ReportDir
New-Item -ItemType Directory -Force -Path $reportDirPath | Out-Null
$changedFiles = @(Expand-CommaSeparatedValues -Values $ChangedFile | ForEach-Object { ([string]$_).Replace('\', '/') } | Select-Object -Unique)
$logPath = Join-Path $reportDirPath "$Check.log"
$jsonPath = Join-Path $reportDirPath "$Check.json"
$mdPath = Join-Path $reportDirPath "$Check.md"

$status = "passed"
$details = @()
$command = ""

try {
    switch ($Check) {
        "changed-file-parse" {
            $command = "covered-by invoke-pre-push-hygiene.ps1"
            $details = @("Changed-file JSON/YAML/PowerShell/Python parse checks are executed by git-hygiene.")
        }
        "protocol-compatibility" {
            $command = "python -m pytest tests/contracts/test_protocol_compatibility.py tests/contracts/test_runtime_gateway_harness_tcp_client_contract.py -q"
            $result = Invoke-Pytest -TestPath @("tests/contracts/test_protocol_compatibility.py", "tests/contracts/test_runtime_gateway_harness_tcp_client_contract.py")
            $result.Output | Set-Content -Path $logPath -Encoding UTF8
            if ($result.ExitCode -ne 0) { throw "pytest exited $($result.ExitCode)" }
            $details = @("Protocol compatibility contract tests passed.")
        }
        "recipe-config-schema" {
            $configCandidates = @($changedFiles | Where-Object { $_ -match '^(config/|data/schemas/|data/.*(recipe|config|machine|parameter|schema))' })
            $schemaFiles = @($configCandidates | Where-Object { $_ -match '^data/schemas/.*\.schema\.json$' })
            $configFiles = @($configCandidates | Where-Object { $_ -match '^config/.*' })
            $unmappedFiles = @($configCandidates | Where-Object {
                ($_ -notmatch '^data/schemas/.*\.schema\.json$') -and
                ($_ -notmatch '^config/machine/machine_config\.ini$')
            })

            if ($unmappedFiles.Count -gt 0) {
                throw "schema compatibility registry missing for changed config/recipe files: $($unmappedFiles -join ', '). Add a formal schema/registry entry before allowing pre-push."
            }

            if ($schemaFiles.Count -gt 0) {
                $schemaProbe = @'
import json
import pathlib
import sys
try:
    import jsonschema
except Exception as exc:
    print(f"jsonschema import failed: {exc}")
    sys.exit(127)
ok = True
root = pathlib.Path(sys.argv[1])
for rel in sys.argv[2:]:
    path = root / rel
    try:
        payload = json.loads(path.read_text(encoding="utf-8-sig"))
        jsonschema.validators.validator_for(payload).check_schema(payload)
        print(f"schema-ok {rel}")
    except Exception as exc:
        ok = False
        print(f"schema-failed {rel}: {exc}")
sys.exit(0 if ok else 1)
'@
                $result = Invoke-PythonScript -Source $schemaProbe -Arguments @($workspaceRoot) + $schemaFiles
                $result.Output | Set-Content -Path $logPath -Encoding UTF8
                if ($result.ExitCode -eq 127) { throw "jsonschema module missing. Run scripts/validation/install-python-deps.ps1." }
                if ($result.ExitCode -ne 0) { throw "schema self-validation failed; see $logPath" }
                $details += @($result.Output -split "`r?`n" | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
            }

            if ($configFiles.Count -gt 0) {
                $iniProbe = @'
import configparser
import pathlib
import sys
ok = True
root = pathlib.Path(sys.argv[1])
for rel in sys.argv[2:]:
    path = root / rel
    try:
        parser = configparser.ConfigParser(strict=True)
        parser.read(path, encoding="utf-8")
        if not parser.sections():
            raise ValueError("no sections parsed")
        print(f"ini-ok {rel}")
    except Exception as exc:
        ok = False
        print(f"ini-failed {rel}: {exc}")
sys.exit(0 if ok else 1)
'@
                $result = Invoke-PythonScript -Source $iniProbe -Arguments @($workspaceRoot) + $configFiles
                Add-Content -Path $logPath -Value $result.Output -Encoding UTF8
                if ($result.ExitCode -ne 0) { throw "config INI compatibility parse failed; see $logPath" }
                $details += @($result.Output -split "`r?`n" | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
            }

            foreach ($file in $configCandidates) {
                $fullPath = Join-Path $workspaceRoot ($file -replace '/', [System.IO.Path]::DirectorySeparatorChar)
                if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) { continue }
                $ext = [System.IO.Path]::GetExtension($fullPath).ToLowerInvariant()
                if ($ext -eq ".json") {
                    Get-Content -Raw -Path $fullPath | ConvertFrom-Json | Out-Null
                    $details += "parsed JSON $file"
                } elseif ($ext -match '^\.ya?ml$') {
                    $details += "YAML parse is covered by git-hygiene for $file"
                }
            }
            if ($details.Count -eq 0) { $details = @("No changed config/schema files required additional parsing.") }
        }
        "state-machine-interlock" {
            $command = "python -m pytest tests/contracts/test_current_chain_validation_contract.py -q"
            $result = Invoke-Pytest -TestPath @("tests/contracts/test_current_chain_validation_contract.py")
            $result.Output | Set-Content -Path $logPath -Encoding UTF8
            if ($result.ExitCode -ne 0) { throw "pytest exited $($result.ExitCode)" }
            $details = @("Current-chain validation contract passed for state/interlock-sensitive changes.")
        }
        "offline-simulation-smoke" {
            $command = "python -m pytest tests/integration/scenarios/first-layer/test_real_dxf_machine_dryrun_observation_contract.py -q"
            $result = Invoke-Pytest -TestPath @("tests/integration/scenarios/first-layer/test_real_dxf_machine_dryrun_observation_contract.py")
            $result.Output | Set-Content -Path $logPath -Encoding UTF8
            if ($result.ExitCode -ne 0) { throw "offline dry-run observation contract exited $($result.ExitCode)" }

            $vendorMarkers = @("MultiCard.dll", "MC_", "vendor runtime", "real motion", "hardware-in-loop")
            $hits = @()
            $markerScanFiles = @(
                $changedFiles | Where-Object {
                    $_ -match '^(apps|modules|shared|tests/(contracts|integration|e2e)|scripts/(?!validation/))/'
                }
            )
            foreach ($file in $markerScanFiles) {
                $fullPath = Join-Path $workspaceRoot ($file -replace '/', [System.IO.Path]::DirectorySeparatorChar)
                if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) { continue }
                $ext = [System.IO.Path]::GetExtension($fullPath).ToLowerInvariant()
                if ($ext -notin @(".ps1", ".py", ".json", ".yaml", ".yml", ".md", ".txt", ".cpp", ".h", ".hpp")) { continue }
                $text = [System.IO.File]::ReadAllText($fullPath)
                foreach ($marker in $vendorMarkers) {
                    if ($text -match [regex]::Escape($marker)) {
                        $hits += "$file contains $marker"
                    }
                }
            }
            if ($hits.Count -gt 0) {
                throw "offline simulation smoke found vendor/hardware dependency markers: $($hits -join '; ')"
            }
            $details = @(
                "Offline dry-run observation contract passed.",
                "Offline simulation smoke found no direct hardware/vendor runtime dependency markers in offline execution surfaces."
            )
        }
        "validation-system-contract" {
            $command = "python -m pytest tests/contracts/test_gate_orchestrator_contract.py tests/contracts/test_strict_runner_gate_contract.py -q"
            $result = Invoke-Pytest -TestPath @("tests/contracts/test_gate_orchestrator_contract.py", "tests/contracts/test_strict_runner_gate_contract.py")
            $result.Output | Set-Content -Path $logPath -Encoding UTF8
            if ($result.ExitCode -ne 0) { throw "pytest exited $($result.ExitCode)" }
            $details = @("Validation/gate contract tests passed.")
        }
        "contracts-quick" {
            $command = "python -m pytest tests/contracts/test_gate_orchestrator_contract.py tests/contracts/test_strict_runner_gate_contract.py tests/contracts/test_layered_validation_lane_policy_contract.py -q"
            $result = Invoke-Pytest -TestPath @(
                "tests/contracts/test_gate_orchestrator_contract.py",
                "tests/contracts/test_strict_runner_gate_contract.py",
                "tests/contracts/test_layered_validation_lane_policy_contract.py"
            )
            $result.Output | Set-Content -Path $logPath -Encoding UTF8
            if ($result.ExitCode -ne 0) { throw "pytest exited $($result.ExitCode)" }
            $details = @("Quick Python contract test subset passed without native build or HIL execution.")
        }
    }
} catch {
    $status = "failed"
    $details += $_.Exception.Message
    if (-not (Test-Path -LiteralPath $logPath)) {
        $_.Exception.Message | Set-Content -Path $logPath -Encoding UTF8
    }
}

$summary = [ordered]@{
    schemaVersion = 1
    generated_at = (Get-Date).ToString("o")
    check = $Check
    status = $status
    command = $command
    changed_files = @($changedFiles)
    details = @($details)
}
$summary | ConvertTo-Json -Depth 10 | Set-Content -Path $jsonPath -Encoding UTF8
@(
    "# Pre-push Quick Check",
    "",
    "- check: ``$Check``",
    "- status: ``$status``",
    "- command: ``$command``",
    "",
    "## Details"
) + @($details | ForEach-Object { "- $_" }) | Set-Content -Path $mdPath -Encoding UTF8

Write-Output "pre-push quick-check=$Check status=$status report=$reportDirPath"
if ($status -ne "passed") { exit 1 }
