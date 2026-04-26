[CmdletBinding()]
param(
    [string]$ReportDir = "tests\reports\import-linter",
    [string]$ConfigPath = ".importlinter"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "tooling-common.ps1")

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$resolvedReportDir = Ensure-WorkspaceDirectory -Path (Resolve-WorkspaceReportPath -WorkspaceRoot $workspaceRoot -ReportPath $ReportDir)
$logsDir = Ensure-WorkspaceDirectory -Path (Join-Path $resolvedReportDir "logs")
$resolvedConfigPath = Resolve-WorkspaceReportPath -WorkspaceRoot $workspaceRoot -ReportPath $ConfigPath

if (-not (Test-Path $resolvedConfigPath)) {
    throw "Import Linter config file was not found: $resolvedConfigPath"
}

$lintImportsCommand = Resolve-WorkspaceToolPath -ToolNames @("lint-imports", "import-linter") -Required
$pathEntries = @(
    (Join-Path $workspaceRoot "apps\hmi-app\src"),
    (Join-Path $workspaceRoot "modules\hmi-application\application"),
    (Join-Path $workspaceRoot "modules\dxf-geometry\application"),
    (Join-Path $workspaceRoot "shared\testing\test-kit\src")
)
$existingPythonPath = $env:PYTHONPATH
$env:PYTHONPATH = (($pathEntries + @($existingPythonPath) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }) -join [IO.Path]::PathSeparator)

$contracts = @(
    @{ Id = "production-no-test-kit"; Name = "production code must not depend on test_kit"; Severity = "hard" },
    @{ Id = "hmi-ui-no-direct-tools"; Name = "UI must not depend directly on fake/mock tool modules"; Severity = "hard" },
    @{ Id = "hmi-ui-no-direct-runtime-session-internals"; Name = "UI must not depend directly on backend_manager/tcp_client/gateway_launch internals"; Severity = "hard" },
    @{ Id = "canonical-hmi-application-no-compat-shell"; Name = "canonical hmi_application must not depend on hmi_client compat shell"; Severity = "advisory" },
    @{ Id = "hmi-ui-no-owner-direct-import"; Name = "UI should not import the hmi_application owner directly"; Severity = "advisory" }
)

$results = @()
$hardFailureCount = 0

try {
    foreach ($contract in $contracts) {
        $logPath = Join-Path $logsDir ("{0}.log" -f $contract.Id)
        $output = & $lintImportsCommand --config $resolvedConfigPath --contract $contract.Id --no-cache 2>&1
        $exitCode = $LASTEXITCODE
        $text = ($output | Out-String).Trim()
        Set-Content -LiteralPath $logPath -Value $text -Encoding UTF8

        $status = if ($exitCode -eq 0) { "passed" } else { "failed" }
        if ($status -eq "failed" -and $contract.Severity -eq "hard") {
            $hardFailureCount += 1
        }

        $results += [PSCustomObject]@{
            id = $contract.Id
            name = $contract.Name
            severity = $contract.Severity
            status = $status
            exit_code = $exitCode
            log = $logPath
            output = $text
        }
    }
}
finally {
    $env:PYTHONPATH = $existingPythonPath
}

$jsonReportPath = Join-Path $resolvedReportDir "import-linter-report.json"
$mdReportPath = Join-Path $resolvedReportDir "import-linter-report.md"
$payload = [PSCustomObject]@{
    gate = "Import Linter"
    config = ".importlinter"
    failure_condition = ">=1 broken hard contract"
    report_dir = "tests/reports/import-linter"
    hard_failure_count = $hardFailureCount
    contracts = $results
}
$payload | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $jsonReportPath -Encoding UTF8

$mdLines = @(
    '# Import Linter Gate',
    '',
    '- config: `.importlinter`',
    '- gate: `hard-fail on hard contracts, advisory on advisory contracts`',
    '- failure_condition: `>=1 broken hard contract`',
    '- json_report: `tests/reports/import-linter/import-linter-report.json`',
    '- markdown_report: `tests/reports/import-linter/import-linter-report.md`',
    ('- hard_failure_count: `{0}`' -f $hardFailureCount),
    ''
)
$mdLines += '| contract_id | severity | status | log |'
$mdLines += '|---|---|---|---|'
$relativeLogById = @{}
foreach ($result in $results) {
    $logPathValue = [string]$result.log
    $relativeLog = if ($logPathValue.StartsWith($workspaceRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        $logPathValue.Substring($workspaceRoot.Length).TrimStart('\')
    }
    else {
        $logPathValue
    }
    $relativeLog = $relativeLog.Replace('\', '/')
    $relativeLogById[[string]$result.id] = $relativeLog
    $mdLines += ('| {0} | {1} | {2} | `{3}` |' -f [string]$result.id, [string]$result.severity, [string]$result.status, $relativeLog)
}

foreach ($result in $results) {
    $mdLines += ''
    $mdLines += "## $([string]$result.id)"
    $mdLines += ''
    $mdLines += "- name: $([string]$result.name)"
    $mdLines += ('- severity: `{0}`' -f [string]$result.severity)
    $mdLines += ('- status: `{0}`' -f [string]$result.status)
    $mdLines += ('- log: `{0}`' -f [string]$relativeLogById[[string]$result.id])
    if (-not [string]::IsNullOrWhiteSpace([string]$result.output)) {
        $mdLines += ''
        $mdLines += '```text'
        $mdLines += [string]$result.output
        $mdLines += '```'
    }
}

Set-Content -LiteralPath $mdReportPath -Value ($mdLines -join "`r`n") -Encoding UTF8

Write-Output "import-linter report json: $jsonReportPath"
Write-Output "import-linter report md:   $mdReportPath"
Write-Output "import-linter hard failures: $hardFailureCount"

foreach ($result in $results) {
    Write-Output ("[{0}] {1} -> {2}" -f [string]$result.severity, [string]$result.id, [string]$result.status)
}

if ($hardFailureCount -gt 0) {
    exit 1
}

exit 0
