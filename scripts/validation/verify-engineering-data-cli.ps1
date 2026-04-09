param(
    [string]$PythonExe,
    [string]$ReportDir = "tests\\reports\\verify\\wave4d-closeout\\launcher",
    [switch]$RequireConsoleScripts
)

$ErrorActionPreference = "Stop"

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if ([System.IO.Path]::IsPathRooted($ReportDir)) {
    $resolvedReportDir = [System.IO.Path]::GetFullPath($ReportDir)
} else {
    $resolvedReportDir = [System.IO.Path]::GetFullPath((Join-Path $workspaceRoot $ReportDir))
}
New-Item -ItemType Directory -Force -Path $resolvedReportDir | Out-Null

if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    if (-not [string]::IsNullOrWhiteSpace($env:SILIGEN_ENGINEERING_DATA_PYTHON)) {
        $PythonExe = $env:SILIGEN_ENGINEERING_DATA_PYTHON
    } else {
        $PythonExe = "python"
    }
}

$pythonPackageRoots = @(
    [System.IO.Path]::GetFullPath((Join-Path $workspaceRoot "modules\dxf-geometry\application")),
    [System.IO.Path]::GetFullPath((Join-Path $workspaceRoot "modules\runtime-execution\application"))
)
$existingPythonPath = $env:PYTHONPATH
$pathSeparator = [System.IO.Path]::PathSeparator
$env:PYTHONPATH = if ([string]::IsNullOrWhiteSpace($existingPythonPath)) {
    $pythonPackageRoots -join $pathSeparator
} else {
    ($pythonPackageRoots + $existingPythonPath) -join $pathSeparator
}

function Invoke-ProcessCapture {
    param(
        [string]$Label,
        [string]$FilePath,
        [string[]]$Arguments
    )

    $lines = & $FilePath @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    $text = ($lines | Out-String).Trim()
    if ([string]::IsNullOrWhiteSpace($text)) {
        $text = "<empty>"
    }

    return [ordered]@{
        label = $Label
        command = (@($FilePath) + $Arguments) -join " "
        exit_code = $exitCode
        output = $text
    }
}

function Invoke-RequiredCheck {
    param(
        [string]$Label,
        [string[]]$Arguments
    )

    $result = Invoke-ProcessCapture -Label $Label -FilePath $PythonExe -Arguments $Arguments
    $result["kind"] = "required"
    $result["passed"] = ($result.exit_code -eq 0)
    return $result
}

function Invoke-ConsoleScriptCheck {
    param(
        [string]$ScriptName
    )

    $result = Invoke-ProcessCapture -Label ("console-script " + $ScriptName) -FilePath "where.exe" -Arguments @($ScriptName)
    $result["kind"] = "console_script"
    $result["script_name"] = $ScriptName
    $result["passed"] = ($result.exit_code -eq 0)
    return $result
}

$checks = @()
$previewScript = Join-Path $workspaceRoot "scripts\engineering-data\generate_preview.py"
$exportSimulationInputScript = Join-Path $workspaceRoot "scripts\engineering-data\export_simulation_input.py"
$trajectoryScript = Join-Path $workspaceRoot "scripts\engineering-data\path_to_trajectory.py"
$checks += Invoke-RequiredCheck -Label "import engineering_data" -Arguments @("-c", "import engineering_data")
$checks += Invoke-RequiredCheck -Label "module dxf_to_pb" -Arguments @("-m", "engineering_data.cli.dxf_to_pb", "--help")
$checks += Invoke-RequiredCheck -Label "module export_simulation_input" -Arguments @("-m", "engineering_data.cli.export_simulation_input", "--help")
$checks += Invoke-RequiredCheck -Label "workspace export_simulation_input" -Arguments @($exportSimulationInputScript, "--help")
$checks += Invoke-RequiredCheck -Label "workspace path_to_trajectory" -Arguments @($trajectoryScript, "--help")
$checks += Invoke-RequiredCheck -Label "workspace generate_preview" -Arguments @($previewScript, "--help")

$consoleChecks = @()
$consoleChecks += Invoke-ConsoleScriptCheck -ScriptName "engineering-data-dxf-to-pb"
$consoleChecks += Invoke-ConsoleScriptCheck -ScriptName "engineering-data-path-to-trajectory"
$consoleChecks += Invoke-ConsoleScriptCheck -ScriptName "engineering-data-export-simulation-input"
$consoleChecks += Invoke-ConsoleScriptCheck -ScriptName "engineering-data-generate-preview"

$requiredPass = @($checks | Where-Object { -not $_.passed }).Count -eq 0
$consolePass = @($consoleChecks | Where-Object { -not $_.passed }).Count -eq 0
$overallPass = $requiredPass -and (($RequireConsoleScripts -and $consolePass) -or (-not $RequireConsoleScripts))

$payload = [ordered]@{
    generated_at = (Get-Date).ToString("o")
    workspace_root = $workspaceRoot
    python_exe = $PythonExe
    pythonpath = $env:PYTHONPATH
    require_console_scripts = $RequireConsoleScripts.IsPresent
    required_checks_passed = $requiredPass
    console_scripts_passed = $consolePass
    overall_status = $(if ($overallPass) { "passed" } else { "failed" })
    checks = @($checks + $consoleChecks)
}

$jsonPath = Join-Path $resolvedReportDir "engineering-data-cli-check.json"
$mdPath = Join-Path $resolvedReportDir "engineering-data-cli-check.md"
$payload | ConvertTo-Json -Depth 8 | Set-Content -Path $jsonPath -Encoding utf8

$mdLines = @()
$mdLines += "# Engineering Data CLI Check"
$mdLines += ""
$mdLines += ('- generated_at: `{0}`' -f $payload.generated_at)
$mdLines += ('- python_exe: `{0}`' -f $payload.python_exe)
$mdLines += ('- pythonpath: `{0}`' -f $payload.pythonpath)
$mdLines += ('- require_console_scripts: `{0}`' -f $payload.require_console_scripts)
$mdLines += ('- required_checks_passed: `{0}`' -f $payload.required_checks_passed)
$mdLines += ('- console_scripts_passed: `{0}`' -f $payload.console_scripts_passed)
$mdLines += ('- overall_status: `{0}`' -f $payload.overall_status)
$mdLines += ""
$mdLines += "## Checks"
$mdLines += ""
$mdLines += "| Label | Kind | Exit Code | Passed |"
$mdLines += "|---|---|---|---|"

foreach ($check in $payload.checks) {
    $mdLines += "| $($check.label) | $($check.kind) | $($check.exit_code) | $($check.passed) |"
}

foreach ($check in $payload.checks) {
    $mdLines += ""
    $mdLines += "## $($check.label)"
    $mdLines += ""
    $mdLines += ('- command: `{0}`' -f $check.command)
    $mdLines += ('- exit_code: `{0}`' -f $check.exit_code)
    $mdLines += ('- passed: `{0}`' -f $check.passed)
    $mdLines += ""
    $mdLines += '```text'
    $mdLines += $check.output
    $mdLines += '```'
}

$mdLines -join "`r`n" | Set-Content -Path $mdPath -Encoding utf8

Write-Output "engineering-data cli check complete: status=$($payload.overall_status)"
Write-Output "json report: $jsonPath"
Write-Output "markdown report: $mdPath"

if (-not $overallPass) {
    exit 1
}
