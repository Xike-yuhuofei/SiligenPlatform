param(
    [int]$TimeoutMs = 20000,
    [string]$PythonExe = "python"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $false

$ExitSuccess = 0
$ExitGuiAssertionFailed = 10
$ExitGuiTimeout = 11

$projectRoot = Split-Path $PSScriptRoot -Parent
$sourceRoot = Join-Path $projectRoot "src"
$uiQtest = Join-Path $sourceRoot "hmi_client\tools\ui_qtest.py"

if ([string]::IsNullOrWhiteSpace($env:PYTHONPATH)) {
    $env:PYTHONPATH = $sourceRoot
} else {
    $env:PYTHONPATH = "$sourceRoot$([IO.Path]::PathSeparator)$env:PYTHONPATH"
}

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

$stdoutLog = Join-Path ([IO.Path]::GetTempPath()) ("siligen-verify-online-recovery-{0}-stdout.log" -f [guid]::NewGuid().ToString("N"))
$stderrLog = Join-Path ([IO.Path]::GetTempPath()) ("siligen-verify-online-recovery-{0}-stderr.log" -f [guid]::NewGuid().ToString("N"))
$args = @(
    $uiQtest,
    "--mode", "online",
    "--timeout-ms", "$TimeoutMs",
    "--exercise-recovery"
)

try {
    Write-Host "[verify-online-recovery-loop] mode=online timeout_ms=$TimeoutMs"
    $process = Start-Process `
        -FilePath $PythonExe `
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

if ($rawExitCode -ne 0) {
    if ($output -match "FAIL: Timed out waiting for GUI contract completion") {
        exit $ExitGuiTimeout
    }
    exit $ExitGuiAssertionFailed
}

if ($output -notmatch "SUPERVISOR_EVENT type=recovery_invoked(\s|$)") {
    Write-Host "[verify-online-recovery-loop] missing SUPERVISOR_EVENT recovery_invoked"
    exit $ExitGuiAssertionFailed
}

if ($output -notmatch "SUPERVISOR_EVENT type=stage_failed .*stage=tcp_ready(\s|$)") {
    Write-Host "[verify-online-recovery-loop] missing SUPERVISOR_EVENT stage_failed for tcp_ready runtime degradation"
    exit $ExitGuiAssertionFailed
}

if ($output -notmatch "SUPERVISOR_EVENT type=stage_succeeded .*stage=online_ready(\s|$)") {
    Write-Host "[verify-online-recovery-loop] missing SUPERVISOR_EVENT stage_succeeded for online_ready after recovery"
    exit $ExitGuiAssertionFailed
}

if ($output -notmatch "SUPERVISOR_DIAG .*online_ready=true(\s|$)") {
    Write-Host "[verify-online-recovery-loop] final supervisor diag does not report online_ready=true"
    exit $ExitGuiAssertionFailed
}

Write-Host "[verify-online-recovery-loop] recovery cycle verified with supervisor events and final online_ready=true"
exit $ExitSuccess
