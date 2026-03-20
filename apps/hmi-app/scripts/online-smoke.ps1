param(
    [int]$TimeoutMs = 15000,
    [int]$MockStartupTimeoutMs = 5000,
    [string]$PythonExe = "python",
    [string]$ListenHost = "127.0.0.1",
    [int]$Port = 0,
    [string]$MockCommand = "",
    [switch]$VerboseMock
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $false

$ExitGuiAssertionFailed = 10
$ExitGuiTimeout = 11
$ExitMockStartFailed = 20
$ExitMockReadyTimeout = 21

$projectRoot = Split-Path $PSScriptRoot -Parent
$sourceRoot = Join-Path $projectRoot "src"
$uiQtest = Join-Path $sourceRoot "hmi_client\\tools\\ui_qtest.py"
$mockServer = Join-Path $sourceRoot "hmi_client\\tools\\mock_server.py"

if ([string]::IsNullOrWhiteSpace($env:PYTHONPATH)) {
    $env:PYTHONPATH = $sourceRoot
} else {
    $env:PYTHONPATH = "$sourceRoot$([IO.Path]::PathSeparator)$env:PYTHONPATH"
}

function Get-FreeTcpPort {
    $listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Parse($ListenHost), 0)
    $listener.Start()
    try {
        return ([System.Net.IPEndPoint]$listener.LocalEndpoint).Port
    } finally {
        $listener.Stop()
    }
}

function Test-TcpPort {
    param(
        [string]$TargetHost,
        [int]$TargetPort
    )

    $client = [System.Net.Sockets.TcpClient]::new()
    try {
        $async = $client.BeginConnect($TargetHost, $TargetPort, $null, $null)
        if (-not $async.AsyncWaitHandle.WaitOne(250)) {
            return $false
        }
        $client.EndConnect($async)
        return $true
    } catch {
        return $false
    } finally {
        $client.Dispose()
    }
}

function Read-LogFile {
    param([string]$Path)

    if (Test-Path $Path) {
        $content = Get-Content $Path -Raw
        if ($null -eq $content) {
            return ""
        }
        return $content.Trim()
    }
    return ""
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

function Stop-MockProcess {
    param([System.Diagnostics.Process]$Process)

    if ($null -eq $Process) {
        return
    }

    if (-not $Process.HasExited) {
        Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
        try {
            Wait-Process -Id $Process.Id -Timeout 2 -ErrorAction Stop
        } catch {
        }
    }
}

function Resolve-MockEntry {
    param([string]$Entry)

    if ([string]::IsNullOrWhiteSpace($Entry)) {
        return $mockServer
    }
    if ([IO.Path]::IsPathRooted($Entry)) {
        return $Entry
    }
    if (Test-Path $Entry) {
        return (Resolve-Path $Entry).Path
    }
    $projectRelativeEntry = Join-Path $projectRoot $Entry
    if (Test-Path $projectRelativeEntry) {
        return (Resolve-Path $projectRelativeEntry).Path
    }
    return $projectRelativeEntry
}

$actualPort = if ($Port -gt 0) { $Port } else { Get-FreeTcpPort }
if (Test-TcpPort -TargetHost $ListenHost -TargetPort $actualPort) {
    Write-Host "[online-smoke] mock port already in use host=$ListenHost port=$actualPort"
    exit $ExitMockStartFailed
}

$mockEntry = Resolve-MockEntry -Entry $MockCommand
$stdoutLog = Join-Path ([IO.Path]::GetTempPath()) ("siligen-online-smoke-{0}-stdout.log" -f [guid]::NewGuid().ToString("N"))
$stderrLog = Join-Path ([IO.Path]::GetTempPath()) ("siligen-online-smoke-{0}-stderr.log" -f [guid]::NewGuid().ToString("N"))
$mockArgs = @("-u", $mockEntry, "--host", $ListenHost, "--port", "$actualPort")
if ($VerboseMock) {
    $mockArgs += "--verbose"
}

$mockProcess = $null

try {
    Write-Host "[online-smoke] starting mock server host=$ListenHost port=$actualPort entry=$mockEntry"
    $mockProcess = Start-Process `
        -FilePath $PythonExe `
        -ArgumentList $mockArgs `
        -PassThru `
        -RedirectStandardOutput $stdoutLog `
        -RedirectStandardError $stderrLog `
        -WindowStyle Hidden

    $deadline = (Get-Date).AddMilliseconds($MockStartupTimeoutMs)
    $ready = $false
    while ((Get-Date) -lt $deadline) {
        if ($mockProcess.HasExited) {
            $stdoutText = Read-LogFile -Path $stdoutLog
            $stderrText = Read-LogFile -Path $stderrLog
            Write-Host "[online-smoke] mock server exited early exit_code=$($mockProcess.ExitCode)"
            if ($stdoutText) {
                Write-Host "[online-smoke][mock-stdout] $stdoutText"
            }
            if ($stderrText) {
                Write-Host "[online-smoke][mock-stderr] $stderrText"
            }
            exit $ExitMockStartFailed
        }

        if (Test-TcpPort -TargetHost $ListenHost -TargetPort $actualPort) {
            $ready = $true
            break
        }

        Start-Sleep -Milliseconds 100
    }

    if (-not $ready) {
        Write-Host "[online-smoke] mock server readiness timeout host=$ListenHost port=$actualPort timeout_ms=$MockStartupTimeoutMs"
        exit $ExitMockReadyTimeout
    }

    $uiArgs = @(
        $uiQtest,
        "--mode", "online",
        "--host", $ListenHost,
        "--port", "$actualPort",
        "--no-mock",
        "--timeout-ms", "$TimeoutMs"
    )

    Write-Host "[online-smoke] mode=online timeout_ms=$TimeoutMs host=$ListenHost port=$actualPort"
    $uiStdoutLog = Join-Path ([IO.Path]::GetTempPath()) ("siligen-online-qtest-{0}-stdout.log" -f [guid]::NewGuid().ToString("N"))
    $uiStderrLog = Join-Path ([IO.Path]::GetTempPath()) ("siligen-online-qtest-{0}-stderr.log" -f [guid]::NewGuid().ToString("N"))

    try {
        $uiProcess = Start-Process `
            -FilePath $PythonExe `
            -ArgumentList $uiArgs `
            -PassThru `
            -Wait `
            -RedirectStandardOutput $uiStdoutLog `
            -RedirectStandardError $uiStderrLog
        $output = Read-CombinedOutput -StdoutPath $uiStdoutLog -StderrPath $uiStderrLog
        $rawExitCode = $uiProcess.ExitCode
    } finally {
        Remove-Item $uiStdoutLog, $uiStderrLog -ErrorAction SilentlyContinue
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
        Write-Host "[online-smoke] failed exit_code=$exitCode"
    }

    exit $exitCode
} finally {
    Stop-MockProcess -Process $mockProcess
    Remove-Item $stdoutLog, $stderrLog -ErrorAction SilentlyContinue
}
