param(
    [int]$TimeoutMs = 15000,
    [string]$PythonExe = "python"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $false

$ExitGuiAssertionFailed = 10
$ExitGuiTimeout = 11

$projectRoot = Split-Path $PSScriptRoot -Parent
$sourceRoot = Join-Path $projectRoot "src"
$uiQtest = Join-Path $sourceRoot "hmi_client\\tools\\ui_qtest.py"

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
        $chunks += (Get-Content $StdoutPath -Raw)
    }
    if (Test-Path $StderrPath) {
        $chunks += (Get-Content $StderrPath -Raw)
    }
    return ($chunks -join [Environment]::NewLine).TrimEnd()
}

$args = @(
    $uiQtest,
    "--mode", "offline",
    "--timeout-ms", "$TimeoutMs"
)

Write-Host "[offline-smoke] mode=offline timeout_ms=$TimeoutMs"
$stdoutLog = Join-Path ([IO.Path]::GetTempPath()) ("siligen-offline-smoke-{0}-stdout.log" -f [guid]::NewGuid().ToString("N"))
$stderrLog = Join-Path ([IO.Path]::GetTempPath()) ("siligen-offline-smoke-{0}-stderr.log" -f [guid]::NewGuid().ToString("N"))

try {
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

$exitCode = 0
if ($rawExitCode -ne 0) {
    if ($output -match "FAIL: Timed out waiting for GUI contract completion") {
        $exitCode = $ExitGuiTimeout
    } else {
        $exitCode = $ExitGuiAssertionFailed
    }
}

if ($exitCode -ne 0) {
    Write-Host "[offline-smoke] failed exit_code=$exitCode"
}

exit $exitCode
