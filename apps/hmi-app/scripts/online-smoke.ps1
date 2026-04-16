param(
    [int]$TimeoutMs = 15000,
    [int]$MockStartupTimeoutMs = 5000,
    [string]$PythonExe = "python",
    [string]$ListenHost = "127.0.0.1",
    [int]$Port = 0,
    [string]$MockCommand = "",
    [string]$LaunchSpecPath = "",
    [string]$GatewayExe = "",
    [string]$GatewayConfig = "",
    [switch]$UseMockGatewayConfig,
    [int]$DoorInputIndex = -1,
    [string]$ExpectFailureCode = "",
    [string]$ExpectFailureStage = "",
    [switch]$UseSupervisorInjection,
    [switch]$ExerciseRuntimeActions,
    [string]$ScreenshotPath = "",
    [string]$PreviewPayloadPath = "",
    [string]$RuntimeActionProfile = "",
    [switch]$VerboseMock
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $false

$ExitGuiAssertionFailed = 10
$ExitGuiTimeout = 11
$ExitMockStartFailed = 20
$ExitMockReadyTimeout = 21

$hasExpectedSupervisorFailure = (-not [string]::IsNullOrWhiteSpace($ExpectFailureCode)) -or (-not [string]::IsNullOrWhiteSpace($ExpectFailureStage))
$effectiveUseSupervisorInjection = $UseSupervisorInjection -or $hasExpectedSupervisorFailure
if ($hasExpectedSupervisorFailure -and -not $UseSupervisorInjection) {
    Write-Host "[online-smoke] ExpectFailure* detected; forcing supervisor injection mode for authoritative diagnostics"
}

$projectRoot = Split-Path $PSScriptRoot -Parent
$workspaceRoot = Split-Path (Split-Path $projectRoot -Parent) -Parent
$sourceRoot = Join-Path $projectRoot "src"
$hmiApplicationRoot = Join-Path $workspaceRoot "modules\hmi-application\application"
$uiQtest = Join-Path $sourceRoot "hmi_client\\tools\\ui_qtest.py"
$mockServer = Join-Path $sourceRoot "hmi_client\\tools\\mock_server.py"
$hangingServer = Join-Path $sourceRoot "hmi_client\\tools\\fake_hanging_server.py"

if ([string]::IsNullOrWhiteSpace($env:PYTHONPATH)) {
    $env:PYTHONPATH = "$hmiApplicationRoot$([IO.Path]::PathSeparator)$sourceRoot"
} else {
    $env:PYTHONPATH = "$hmiApplicationRoot$([IO.Path]::PathSeparator)$sourceRoot$([IO.Path]::PathSeparator)$env:PYTHONPATH"
}

function Assert-RuntimeActionParameterContract {
    param(
        [switch]$ExerciseRuntimeActions,
        [string]$PreviewPayloadPath,
        [string]$RuntimeActionProfile
    )

    $hasPayload = -not [string]::IsNullOrWhiteSpace($PreviewPayloadPath)
    $hasProfile = -not [string]::IsNullOrWhiteSpace($RuntimeActionProfile)
    $normalizedProfile = $RuntimeActionProfile.Trim().ToLowerInvariant()

    if ($hasPayload -and -not $ExerciseRuntimeActions) {
        throw "PreviewPayloadPath requires -ExerciseRuntimeActions and -RuntimeActionProfile snapshot_render"
    }
    if ($hasProfile -and -not $ExerciseRuntimeActions) {
        throw "RuntimeActionProfile requires -ExerciseRuntimeActions"
    }
    if (-not $ExerciseRuntimeActions) {
        return
    }
    if ($hasPayload -and -not $hasProfile) {
        throw "PreviewPayloadPath requires explicit -RuntimeActionProfile snapshot_render; implicit fallback is forbidden"
    }
    if ($hasPayload -and $normalizedProfile -ne "snapshot_render") {
        throw "PreviewPayloadPath is only allowed with -RuntimeActionProfile snapshot_render"
    }
    if ($normalizedProfile -eq "snapshot_render" -and -not $hasPayload) {
        throw "RuntimeActionProfile snapshot_render requires -PreviewPayloadPath"
    }
}

$normalizedRuntimeActionProfile = $RuntimeActionProfile.Trim().ToLowerInvariant()
Assert-RuntimeActionParameterContract `
    -ExerciseRuntimeActions:$ExerciseRuntimeActions `
    -PreviewPayloadPath $PreviewPayloadPath `
    -RuntimeActionProfile $normalizedRuntimeActionProfile

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

function New-TemporaryLaunchSpec {
    param(
        [string]$PythonPath,
        [string]$EntryPath,
        [string]$TargetHost,
        [int]$TargetPort
    )

    $specPath = Join-Path ([IO.Path]::GetTempPath()) ("siligen-gateway-launch-{0}.json" -f [guid]::NewGuid().ToString("N"))
    $resolvedPython = $PythonPath
    if (-not [IO.Path]::IsPathRooted($resolvedPython)) {
        try {
            $resolvedPython = (Get-Command $PythonPath -ErrorAction Stop).Source
        } catch {
            throw "Cannot resolve python executable for launch spec: $PythonPath"
        }
    }

    $payload = @{
        executable = $resolvedPython
        args       = @("-u", $EntryPath, "--host", $TargetHost, "--port", "$TargetPort")
    }
    $json = $payload | ConvertTo-Json -Depth 3
    [System.IO.File]::WriteAllText($specPath, $json, [System.Text.UTF8Encoding]::new($false))
    return $specPath
}

function New-TemporaryGatewayConfig {
    param(
        [string]$SourcePath,
        [switch]$EnableMockMode,
        [int]$DoorInput = -1
    )

    if ([string]::IsNullOrWhiteSpace($SourcePath)) {
        $SourcePath = Join-Path $workspaceRoot "config\\machine\\machine_config.ini"
    }

    $resolvedSource = if ([IO.Path]::IsPathRooted($SourcePath)) {
        $SourcePath
    } else {
        (Join-Path $workspaceRoot $SourcePath)
    }
    if (-not (Test-Path $resolvedSource)) {
        throw "Cannot find gateway config source: $resolvedSource"
    }

    $content = Get-Content $resolvedSource -Raw
    $lines = $content -split "(`r`n|`n|`r)"
    $inHardware = $false
    $inInterlock = $false
    $modeReplaced = -not $EnableMockMode
    $doorReplaced = ($DoorInput -lt 0)

    for ($i = 0; $i -lt $lines.Length; $i++) {
        $line = $lines[$i]
        $trimmed = $line.Trim()
        if ($trimmed.StartsWith("[") -and $trimmed.EndsWith("]")) {
            $section = $trimmed.ToLowerInvariant()
            $inHardware = $section -eq "[hardware]"
            $inInterlock = $section -eq "[interlock]"
            continue
        }
        if ($EnableMockMode -and $inHardware -and $trimmed.ToLowerInvariant().StartsWith("mode=")) {
            $indentLength = $line.Length - $line.TrimStart().Length
            $indent = $line.Substring(0, $indentLength)
            $lines[$i] = "$indent" + "mode=Mock"
            $modeReplaced = $true
            continue
        }
        if ($DoorInput -ge 0 -and $inInterlock -and $trimmed.ToLowerInvariant().StartsWith("safety_door_input=")) {
            $indentLength = $line.Length - $line.TrimStart().Length
            $indent = $line.Substring(0, $indentLength)
            $lines[$i] = "$indent" + "safety_door_input=$DoorInput"
            $doorReplaced = $true
        }
    }

    if (-not $modeReplaced) {
        throw "Failed to locate [Hardware] mode entry in gateway config"
    }
    if (-not $doorReplaced) {
        throw "Failed to locate [Interlock] safety_door_input entry in gateway config"
    }

    $tempPath = Join-Path ([IO.Path]::GetTempPath()) ("siligen-gateway-config-{0}.ini" -f [guid]::NewGuid().ToString("N"))
    [System.IO.File]::WriteAllText($tempPath, ($lines -join [Environment]::NewLine), [System.Text.UTF8Encoding]::new($false))
    return $tempPath
}

function New-TemporaryGatewayLaunchSpec {
    param(
        [string]$ExecutablePath,
        [string]$ConfigPath,
        [string]$TargetHost,
        [int]$TargetPort
    )

    if ([string]::IsNullOrWhiteSpace($ExecutablePath)) {
        throw "Gateway executable path is required"
    }

    $resolvedExe = if ([IO.Path]::IsPathRooted($ExecutablePath)) {
        $ExecutablePath
    } else {
        (Join-Path $workspaceRoot $ExecutablePath)
    }
    if (-not (Test-Path $resolvedExe)) {
        throw "Cannot find gateway executable: $resolvedExe"
    }

    $exeItem = Get-Item $resolvedExe
    $exeDir = $exeItem.DirectoryName
    $cwdPath = $workspaceRoot
    $pathEntries = @($exeDir)
    if ($exeItem.Directory.Parent -and $exeItem.Directory.Parent.Name -ieq "bin") {
        $libDir = Join-Path $exeItem.Directory.Parent.Parent.FullName ("lib\\" + $exeItem.Directory.Name)
        if (Test-Path $libDir) {
            $pathEntries += $libDir
        }
    }

    $args = @("--port", "$TargetPort")
    if (-not [string]::IsNullOrWhiteSpace($ConfigPath)) {
        $resolvedConfig = if ([IO.Path]::IsPathRooted($ConfigPath)) {
            $ConfigPath
        } else {
            (Join-Path $workspaceRoot $ConfigPath)
        }
        if (-not (Test-Path $resolvedConfig)) {
            throw "Cannot find gateway config: $resolvedConfig"
        }
        $args = @("--config", $resolvedConfig, "--port", "$TargetPort")
    }

    $payload = @{
        executable  = $exeItem.FullName
        cwd         = $cwdPath
        args        = $args
        pathEntries = $pathEntries
        env         = @{
            SILIGEN_TCP_SERVER_HOST = $TargetHost
            SILIGEN_TCP_SERVER_PORT = "$TargetPort"
        }
    }
    $specPath = Join-Path ([IO.Path]::GetTempPath()) ("siligen-runtime-launch-{0}.json" -f [guid]::NewGuid().ToString("N"))
    $json = $payload | ConvertTo-Json -Depth 5
    [System.IO.File]::WriteAllText($specPath, $json, [System.Text.UTF8Encoding]::new($false))
    return $specPath
}

function Invoke-UiSmoke {
    param(
        [int]$TargetPort,
        [string]$LaunchSpecPath = ""
    )

    $uiArgs = @(
        $uiQtest,
        "--mode", "online",
        "--host", $ListenHost,
        "--port", "$TargetPort",
        "--no-mock",
        "--timeout-ms", "$TimeoutMs"
    )
    if (-not [string]::IsNullOrWhiteSpace($ExpectFailureCode)) {
        $uiArgs += @("--expect-failure-code", $ExpectFailureCode)
    }
    if (-not [string]::IsNullOrWhiteSpace($ExpectFailureStage)) {
        $uiArgs += @("--expect-failure-stage", $ExpectFailureStage)
    }
    if (-not [string]::IsNullOrWhiteSpace($ScreenshotPath)) {
        $uiArgs += @("--screenshot-path", $ScreenshotPath)
    }
    if ($ExerciseRuntimeActions) {
        $uiArgs += "--exercise-runtime-actions"
        if (-not [string]::IsNullOrWhiteSpace($PreviewPayloadPath)) {
            $uiArgs += @("--preview-payload-path", $PreviewPayloadPath)
        }
        if (-not [string]::IsNullOrWhiteSpace($normalizedRuntimeActionProfile)) {
            $uiArgs += @("--runtime-action-profile", $normalizedRuntimeActionProfile)
        }
    }

    $uiStdoutLog = Join-Path ([IO.Path]::GetTempPath()) ("siligen-online-qtest-{0}-stdout.log" -f [guid]::NewGuid().ToString("N"))
    $uiStderrLog = Join-Path ([IO.Path]::GetTempPath()) ("siligen-online-qtest-{0}-stderr.log" -f [guid]::NewGuid().ToString("N"))

    $previousLaunchSpec = $env:SILIGEN_GATEWAY_LAUNCH_SPEC
    $previousAutoStart = $env:SILIGEN_GATEWAY_AUTOSTART
    try {
        if (-not [string]::IsNullOrWhiteSpace($LaunchSpecPath)) {
            $env:SILIGEN_GATEWAY_LAUNCH_SPEC = $LaunchSpecPath
            $env:SILIGEN_GATEWAY_AUTOSTART = "1"
        }
        Write-Host "[online-smoke] mode=online timeout_ms=$TimeoutMs host=$ListenHost port=$TargetPort"
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
        if (-not [string]::IsNullOrWhiteSpace($LaunchSpecPath)) {
            if ([string]::IsNullOrWhiteSpace($previousLaunchSpec)) {
                Remove-Item Env:SILIGEN_GATEWAY_LAUNCH_SPEC -ErrorAction SilentlyContinue
            } else {
                $env:SILIGEN_GATEWAY_LAUNCH_SPEC = $previousLaunchSpec
            }
            if ([string]::IsNullOrWhiteSpace($previousAutoStart)) {
                Remove-Item Env:SILIGEN_GATEWAY_AUTOSTART -ErrorAction SilentlyContinue
            } else {
                $env:SILIGEN_GATEWAY_AUTOSTART = $previousAutoStart
            }
        }
    }

    if ($output) {
        Write-Host $output
    }
    return @{
        RawExitCode = $rawExitCode
        Output      = $output
    }
}

function Resolve-UiExitCode {
    param(
        [int]$RawExitCode,
        [string]$Output
    )

    if ($RawExitCode -ne 0) {
        if ($Output -match "FAIL: Timed out waiting for GUI contract completion") {
            return $ExitGuiTimeout
        }
        return $ExitGuiAssertionFailed
    }

    if ($hasExpectedSupervisorFailure) {
        $expectedCodeFound = $true
        if (-not [string]::IsNullOrWhiteSpace($ExpectFailureCode)) {
            $expectedCodeFound = $Output -match "SUPERVISOR_DIAG .*failure_code=$([regex]::Escape($ExpectFailureCode))(\s|$)"
        }
        $expectedStageFound = $true
        $expectedStageFailedEvent = $true
        if (-not [string]::IsNullOrWhiteSpace($ExpectFailureStage)) {
            $expectedStageFound = $Output -match "SUPERVISOR_DIAG .*failure_stage=$([regex]::Escape($ExpectFailureStage))(\s|$)"
            $expectedStageFailedEvent = $Output -match "SUPERVISOR_EVENT type=stage_failed .*stage=$([regex]::Escape($ExpectFailureStage))(\s|$)"
        }
        if ($expectedCodeFound -and $expectedStageFound -and $expectedStageFailedEvent) {
            return $ExitMockReadyTimeout
        }
        Write-Host "[online-smoke] expected diagnostics not found code=$ExpectFailureCode stage=$ExpectFailureStage"
        return $ExitGuiAssertionFailed
    }

    $requiredSequence = @(
        "backend_starting",
        "backend_ready",
        "tcp_connecting",
        "hardware_probing",
        "online_ready"
    )
    $cursor = 0
    $lines = $Output -split "(`r`n|`n|`r)"
    foreach ($line in $lines) {
        if ($cursor -ge $requiredSequence.Count) {
            break
        }
        $stage = $requiredSequence[$cursor]
        if ($line -match "SUPERVISOR_EVENT type=stage_entered .*stage=$([regex]::Escape($stage))(\s|$)") {
            $cursor++
        }
    }
    if ($cursor -lt $requiredSequence.Count) {
        Write-Host "[online-smoke] missing required stage_entered sequence in SUPERVISOR_EVENT output"
        return $ExitGuiAssertionFailed
    }
    if ($Output -notmatch "SUPERVISOR_DIAG .*online_ready=true(\s|$)") {
        Write-Host "[online-smoke] missing SUPERVISOR_DIAG online_ready=true on success path"
        return $ExitGuiAssertionFailed
    }

    return 0
}

$actualPort = if ($Port -gt 0) { $Port } else { Get-FreeTcpPort }
if (Test-TcpPort -TargetHost $ListenHost -TargetPort $actualPort) {
    Write-Host "[online-smoke] mock port already in use host=$ListenHost port=$actualPort"
    Write-Host "[online-smoke][diagnostics] failure_code=SUP_BACKEND_START_FAILED failure_stage=backend_starting recoverable=true"
    exit $ExitMockStartFailed
}

$mockProcess = $null
$stdoutLog = $null
$stderrLog = $null
$launchSpecPath = $null
$temporaryGatewayConfigPath = $null
$ownsLaunchSpecPath = $false

try {
    if (-not [string]::IsNullOrWhiteSpace($GatewayExe)) {
        $resolvedGatewayConfig = $GatewayConfig
        $effectiveDoorInputIndex = $DoorInputIndex
        if ($ExerciseRuntimeActions -and $UseMockGatewayConfig -and $effectiveDoorInputIndex -lt 0) {
            $effectiveDoorInputIndex = 0
            Write-Host "[online-smoke] enabling safety_door_input=$effectiveDoorInputIndex for runtime action smoke"
        }
        if ($UseMockGatewayConfig -or $effectiveDoorInputIndex -ge 0) {
            $temporaryGatewayConfigPath = New-TemporaryGatewayConfig `
                -SourcePath $GatewayConfig `
                -EnableMockMode:$UseMockGatewayConfig `
                -DoorInput $effectiveDoorInputIndex
            $resolvedGatewayConfig = $temporaryGatewayConfigPath
        }
        $launchSpecPath = New-TemporaryGatewayLaunchSpec `
            -ExecutablePath $GatewayExe `
            -ConfigPath $resolvedGatewayConfig `
            -TargetHost $ListenHost `
            -TargetPort $actualPort
        $ownsLaunchSpecPath = $true
        Write-Host "[online-smoke] runtime gateway mode enabled host=$ListenHost port=$actualPort exe=$GatewayExe"
        if (-not [string]::IsNullOrWhiteSpace($resolvedGatewayConfig)) {
            Write-Host "[online-smoke] runtime gateway config=$resolvedGatewayConfig"
        }
        $uiResult = Invoke-UiSmoke -TargetPort $actualPort -LaunchSpecPath $launchSpecPath
    } elseif (-not [string]::IsNullOrWhiteSpace($LaunchSpecPath)) {
        $launchSpecPath = $LaunchSpecPath
        Write-Host "[online-smoke] explicit launch spec mode enabled host=$ListenHost port=$actualPort spec=$launchSpecPath"
        $uiResult = Invoke-UiSmoke -TargetPort $actualPort -LaunchSpecPath $launchSpecPath
    } elseif ($effectiveUseSupervisorInjection) {
        $injectSource = if ([string]::IsNullOrWhiteSpace($MockCommand)) { $hangingServer } else { $MockCommand }
        $injectEntry = Resolve-MockEntry -Entry $injectSource
        Write-Host "[online-smoke] supervisor injection mode enabled host=$ListenHost port=$actualPort entry=$injectEntry"
        $launchSpecPath = New-TemporaryLaunchSpec -PythonPath $PythonExe -EntryPath $injectEntry -TargetHost $ListenHost -TargetPort $actualPort
        $ownsLaunchSpecPath = $true
        $uiResult = Invoke-UiSmoke -TargetPort $actualPort -LaunchSpecPath $launchSpecPath
    } else {
        $mockEntry = Resolve-MockEntry -Entry $MockCommand
        $stdoutLog = Join-Path ([IO.Path]::GetTempPath()) ("siligen-online-smoke-{0}-stdout.log" -f [guid]::NewGuid().ToString("N"))
        $stderrLog = Join-Path ([IO.Path]::GetTempPath()) ("siligen-online-smoke-{0}-stderr.log" -f [guid]::NewGuid().ToString("N"))
        $mockArgs = @("-u", $mockEntry, "--host", $ListenHost, "--port", "$actualPort")
        if ($VerboseMock) {
            $mockArgs += "--verbose"
        }

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
                Write-Host "[online-smoke][diagnostics] failure_code=SUP_BACKEND_START_FAILED failure_stage=backend_starting recoverable=true"
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
            Write-Host "[online-smoke][diagnostics] failure_code=SUP_BACKEND_READY_TIMEOUT failure_stage=backend_ready recoverable=true"
            exit $ExitMockReadyTimeout
        }

        $uiResult = Invoke-UiSmoke -TargetPort $actualPort
    }

    $exitCode = Resolve-UiExitCode -RawExitCode $uiResult.RawExitCode -Output $uiResult.Output

    if ($exitCode -ne 0) {
        Write-Host "[online-smoke] failed exit_code=$exitCode"
    }

    exit $exitCode
} finally {
    Stop-MockProcess -Process $mockProcess
    if ($null -ne $stdoutLog -and $null -ne $stderrLog) {
        Remove-Item $stdoutLog, $stderrLog -ErrorAction SilentlyContinue
    }
    if ($ownsLaunchSpecPath -and -not [string]::IsNullOrWhiteSpace($launchSpecPath)) {
        Remove-Item $launchSpecPath -ErrorAction SilentlyContinue
    }
    if (-not [string]::IsNullOrWhiteSpace($temporaryGatewayConfigPath)) {
        Remove-Item $temporaryGatewayConfigPath -ErrorAction SilentlyContinue
    }
}
