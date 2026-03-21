param(
    [int]$MockStartupTimeoutMs = 2000,
    [string]$PythonExe = "python"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $false

$ExitSuccess = 0
$ExitUnexpectedResult = 30
$ExitMissingTimeoutMessage = 31
$ExitCleanupFailed = 32

$projectRoot = Split-Path $PSScriptRoot -Parent
$onlineSmoke = Join-Path $projectRoot "scripts\online-smoke.ps1"
$hangingMock = Join-Path $projectRoot "src\hmi_client\tools\fake_hanging_server.py"

function Read-CombinedOutput {
    param(
        [string]$StdoutPath,
        [string]$StderrPath
    )

    $chunks = @()
    if (Test-Path $StdoutPath) {
        $content = Get-Content $StdoutPath -Raw
        if ($null -ne $content) {
            $chunks += $content
        }
    }
    if (Test-Path $StderrPath) {
        $content = Get-Content $StderrPath -Raw
        if ($null -ne $content) {
            $chunks += $content
        }
    }
    return ($chunks -join [Environment]::NewLine).TrimEnd()
}

function Get-HangingMockProcesses {
    return @(
        Get-CimInstance Win32_Process | Where-Object {
            $_.Name -like "python*" -and $_.CommandLine -like "*fake_hanging_server.py*"
        }
    )
}

function Stop-HangingMockProcesses {
    foreach ($process in Get-HangingMockProcesses) {
        Stop-Process -Id $process.ProcessId -Force -ErrorAction SilentlyContinue
    }
}

Stop-HangingMockProcesses

$stdoutLog = Join-Path ([IO.Path]::GetTempPath()) ("siligen-verify-online-timeout-{0}-stdout.log" -f [guid]::NewGuid().ToString("N"))
$stderrLog = Join-Path ([IO.Path]::GetTempPath()) ("siligen-verify-online-timeout-{0}-stderr.log" -f [guid]::NewGuid().ToString("N"))
$args = @(
    "-ExecutionPolicy", "Bypass",
    "-File", $onlineSmoke,
    "-UseSupervisorInjection",
    "-MockCommand", $hangingMock,
    "-ExpectFailureCode", "SUP_BACKEND_READY_TIMEOUT",
    "-ExpectFailureStage", "backend_ready",
    "-PythonExe", $PythonExe
)

try {
    Write-Host "[verify-online-ready-timeout] expect_exit=21 via supervisor injection timeout_ms=$MockStartupTimeoutMs"
    $process = Start-Process `
        -FilePath "powershell" `
        -ArgumentList $args `
        -PassThru `
        -Wait `
        -RedirectStandardOutput $stdoutLog `
        -RedirectStandardError $stderrLog
    $output = Read-CombinedOutput -StdoutPath $stdoutLog -StderrPath $stderrLog
    $rawExitCode = $process.ExitCode
} finally {
    Remove-Item $stdoutLog, $stderrLog -ErrorAction SilentlyContinue
}

if ($output) {
    Write-Host $output
}

if ($rawExitCode -ne 21) {
    Write-Host "[verify-online-ready-timeout] expected exit_code=21 actual=$rawExitCode"
    Stop-HangingMockProcesses
    exit $ExitUnexpectedResult
}

if ($output -notmatch "SUPERVISOR_DIAG .*failure_code=SUP_BACKEND_READY_TIMEOUT(\s|$)") {
    Write-Host "[verify-online-ready-timeout] missing SUPERVISOR_DIAG failure_code mapping for backend ready timeout"
    Stop-HangingMockProcesses
    exit $ExitMissingTimeoutMessage
}

if ($output -notmatch "SUPERVISOR_DIAG .*failure_stage=backend_ready(\s|$)") {
    Write-Host "[verify-online-ready-timeout] missing SUPERVISOR_DIAG failure_stage mapping for backend ready timeout"
    Stop-HangingMockProcesses
    exit $ExitMissingTimeoutMessage
}

if ($output -notmatch "SUPERVISOR_EVENT type=stage_failed .*stage=backend_ready(\s|$)") {
    Write-Host "[verify-online-ready-timeout] missing SUPERVISOR_EVENT stage_failed mapping for backend ready timeout"
    Stop-HangingMockProcesses
    exit $ExitMissingTimeoutMessage
}

$leftovers = Get-HangingMockProcesses
if (@($leftovers).Count -gt 0) {
    Write-Host "[verify-online-ready-timeout] leftover fake_hanging_server.py process count=$(@($leftovers).Count)"
    Stop-HangingMockProcesses
    exit $ExitCleanupFailed
}

Write-Host "[verify-online-ready-timeout] observed exit_code=21 with expected supervisor failure mapping and clean process teardown"
exit $ExitSuccess
