[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$BuildConfig = "Debug",
    [string]$ConfigPath = "config/machine/machine_config.ini",
    [string]$VendorDir,
    [int]$GatewayPort = 9527,
    [string]$ReportRoot = "tests/reports/hardware-smoke-observation",
    [switch]$RegisterObservation,
    [string]$Operator,
    [string]$MachineId = "unknown-machine"
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "tooling-common.ps1")

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$defaultVendorDir = Join-Path $workspaceRoot "modules\runtime-execution\adapters\device\vendor\multicard"
$runtimeServiceScript = Join-Path $workspaceRoot "apps\runtime-service\run.ps1"
$runtimeGatewayScript = Join-Path $workspaceRoot "apps\runtime-gateway\run.ps1"
$registerScript = Join-Path $PSScriptRoot "register-hardware-smoke-observation.ps1"

function Resolve-FullPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PathValue
    )

    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return [System.IO.Path]::GetFullPath($PathValue)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $workspaceRoot $PathValue))
}

function Get-DisplayPath {
    param([string]$PathValue)

    if ([string]::IsNullOrWhiteSpace($PathValue)) {
        return "<none>"
    }

    $fullPath = [System.IO.Path]::GetFullPath($PathValue)
    $workspaceFullPath = [System.IO.Path]::GetFullPath($workspaceRoot)
    if ($fullPath.StartsWith($workspaceFullPath, [System.StringComparison]::OrdinalIgnoreCase)) {
        $relative = $fullPath.Substring($workspaceFullPath.Length).TrimStart('\', '/')
        if ([string]::IsNullOrWhiteSpace($relative)) {
            return "."
        }

        return $relative -replace '/', '\'
    }

    return $fullPath
}

function Get-IniValue {
    param(
        [Parameter(Mandatory = $true)]
        [string]$IniPath,
        [Parameter(Mandatory = $true)]
        [string]$Section,
        [Parameter(Mandatory = $true)]
        [string]$Key
    )

    $currentSection = ""
    foreach ($line in Get-Content -Path $IniPath) {
        $trimmed = $line.Trim()
        if (($trimmed.Length -eq 0) -or $trimmed.StartsWith("#") -or $trimmed.StartsWith(";")) {
            continue
        }

        if ($trimmed -match '^\[(.+)\]$') {
            $currentSection = $Matches[1]
            continue
        }

        if (($currentSection -eq $Section) -and ($trimmed -match '^([^=]+)=(.*)$')) {
            if ($Matches[1].Trim() -eq $Key) {
                return $Matches[2].Trim()
            }
        }
    }

    return $null
}

function Find-FirstExistingPath {
    param([string[]]$Candidates)

    foreach ($candidate in $Candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    return $null
}

function Get-ControlAppsExecutableCandidates {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$BuildRoots,
        [Parameter(Mandatory = $true)]
        [string]$ExecutableName,
        [Parameter(Mandatory = $true)]
        [string]$BuildConfig
    )

    $candidates = @()
    foreach ($buildRoot in $BuildRoots) {
        $candidates += @(
            (Join-Path $buildRoot "bin\$BuildConfig\$ExecutableName"),
            (Join-Path $buildRoot "bin\$ExecutableName"),
            (Join-Path $buildRoot "bin\Debug\$ExecutableName"),
            (Join-Path $buildRoot "bin\Release\$ExecutableName"),
            (Join-Path $buildRoot "bin\RelWithDebInfo\$ExecutableName")
        )
    }

    return @($candidates | Select-Object -Unique)
}

function Invoke-ExternalProcessCapture {
    param(
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
        exit_code = $exitCode
        output = $text
        command = (@($FilePath) + $Arguments) -join " "
    }
}

function New-Checkpoint {
    param(
        [string]$Id,
        [string]$Category,
        [string]$Status,
        [string]$Detail,
        [string]$Evidence
    )

    return [ordered]@{
        id = $Id
        category = $Category
        status = $Status
        detail = $Detail
        evidence = $Evidence
    }
}

function Get-DryRunFailureCategory {
    param([string]$OutputText)

    if ($OutputText -match "canonical machine config not found") {
        return "config"
    }

    if (($OutputText -match "MultiCard vendor directory not found") -or
        ($OutputText -match "required runtime asset is missing") -or
        ($OutputText -match "MultiCard\.dll")) {
        return "asset"
    }

    if (($OutputText -match "executable not found under") -or
        ($OutputText -match "未知参数") -or
        ($OutputText -match "Cannot find path")) {
        return "owner-boundary"
    }

    return "owner-boundary"
}

function Invoke-AppDryRunCheck {
    param(
        [string]$ScriptPath,
        [string[]]$BaseArguments,
        [string]$StrictLogPath,
        [string]$DiagnosticLogPath
    )

    if (-not (Test-Path $ScriptPath)) {
        $message = "script not found: $ScriptPath"
        $message | Set-Content -Path $StrictLogPath -Encoding utf8
        $message | Set-Content -Path $DiagnosticLogPath -Encoding utf8
        return [ordered]@{
            strict = [ordered]@{
                exit_code = 1
                output = $message
                command = "<script missing>"
            }
            diagnostic = [ordered]@{
                exit_code = 1
                output = $message
                command = "<script missing>"
            }
            classification = "owner-boundary"
        }
    }

    $strictArguments = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $ScriptPath) + $BaseArguments + @("-DryRun")
    $strictResult = Invoke-ExternalProcessCapture -FilePath "powershell" -Arguments $strictArguments
    $strictResult.output | Set-Content -Path $StrictLogPath -Encoding utf8

    $diagnosticArguments = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $ScriptPath) + $BaseArguments + @("-DryRun", "-SkipPreflight")
    $diagnosticResult = Invoke-ExternalProcessCapture -FilePath "powershell" -Arguments $diagnosticArguments
    $diagnosticResult.output | Set-Content -Path $DiagnosticLogPath -Encoding utf8

    $classification = if ($diagnosticResult.exit_code -ne 0) {
        "owner-boundary"
    } elseif ($strictResult.exit_code -ne 0) {
        Get-DryRunFailureCategory -OutputText $strictResult.output
    } else {
        "none"
    }

    return [ordered]@{
        strict = $strictResult
        diagnostic = $diagnosticResult
        classification = $classification
    }
}

$resolvedConfigPath = Resolve-FullPath -PathValue $ConfigPath
$resolvedVendorDir = if (-not [string]::IsNullOrWhiteSpace($VendorDir)) {
    Resolve-FullPath -PathValue $VendorDir
} elseif (-not [string]::IsNullOrWhiteSpace($env:SILIGEN_MULTICARD_VENDOR_DIR)) {
    Resolve-FullPath -PathValue $env:SILIGEN_MULTICARD_VENDOR_DIR
} else {
    $defaultVendorDir
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$resolvedReportRoot = Resolve-FullPath -PathValue $ReportRoot
$runDir = Join-Path $resolvedReportRoot $timestamp
$logsDir = Join-Path $runDir "logs"
New-Item -ItemType Directory -Force -Path $logsDir | Out-Null

$controlAppsBuildRoots = @(Get-ControlAppsBuildRoots -WorkspaceRoot $workspaceRoot)
$controlAppsBuildRoot = $controlAppsBuildRoots[0]

$runtimeServiceExe = Find-FirstExistingPath -Candidates (Get-ControlAppsExecutableCandidates `
    -BuildRoots $controlAppsBuildRoots `
    -ExecutableName "siligen_runtime_service.exe" `
    -BuildConfig $BuildConfig)

$runtimeGatewayExe = Find-FirstExistingPath -Candidates (Get-ControlAppsExecutableCandidates `
    -BuildRoots $controlAppsBuildRoots `
    -ExecutableName "siligen_runtime_gateway.exe" `
    -BuildConfig $BuildConfig)

$configExists = Test-Path $resolvedConfigPath
$vendorDirExists = Test-Path $resolvedVendorDir
$vendorDllPath = Join-Path $resolvedVendorDir "MultiCard.dll"
$vendorLibPath = Join-Path $resolvedVendorDir "MultiCard.lib"
$vendorDllExists = Test-Path $vendorDllPath
$vendorLibExists = Test-Path $vendorLibPath

$hardwareMode = $null
$controlCardIp = $null
$localIp = $null
$networkTimeout = $null
$velocityTraceOutput = $null
$debugLogFile = $null
$emergencyStopInput = $null
$emergencyStopActiveLow = $null
$safetyDoorInput = $null
$axis1Direction = $null
$axis2Direction = $null
$axis1SearchDistance = $null
$axis2SearchDistance = $null

if ($configExists) {
    $hardwareMode = Get-IniValue -IniPath $resolvedConfigPath -Section "Hardware" -Key "mode"
    $controlCardIp = Get-IniValue -IniPath $resolvedConfigPath -Section "Network" -Key "control_card_ip"
    $localIp = Get-IniValue -IniPath $resolvedConfigPath -Section "Network" -Key "local_ip"
    $networkTimeout = Get-IniValue -IniPath $resolvedConfigPath -Section "Network" -Key "timeout_ms"
    $velocityTraceOutput = Get-IniValue -IniPath $resolvedConfigPath -Section "VelocityTrace" -Key "output_path"
    $debugLogFile = Get-IniValue -IniPath $resolvedConfigPath -Section "Debug" -Key "log_file"
    $emergencyStopInput = Get-IniValue -IniPath $resolvedConfigPath -Section "Interlock" -Key "emergency_stop_input"
    $emergencyStopActiveLow = Get-IniValue -IniPath $resolvedConfigPath -Section "Interlock" -Key "emergency_stop_active_low"
    $safetyDoorInput = Get-IniValue -IniPath $resolvedConfigPath -Section "Interlock" -Key "safety_door_input"
    $axis1Direction = Get-IniValue -IniPath $resolvedConfigPath -Section "Homing_Axis1" -Key "direction"
    $axis2Direction = Get-IniValue -IniPath $resolvedConfigPath -Section "Homing_Axis2" -Key "direction"
    $axis1SearchDistance = Get-IniValue -IniPath $resolvedConfigPath -Section "Homing_Axis1" -Key "search_distance"
    $axis2SearchDistance = Get-IniValue -IniPath $resolvedConfigPath -Section "Homing_Axis2" -Key "search_distance"
}

$runtimeServiceCommandText = ".\apps\runtime-service\run.ps1 -BuildConfig $BuildConfig -ConfigPath ""$(Get-DisplayPath -PathValue $resolvedConfigPath)"" -VendorDir ""$(Get-DisplayPath -PathValue $resolvedVendorDir)"""
$runtimeGatewayCommandText = ".\apps\runtime-gateway\run.ps1 -BuildConfig $BuildConfig -ConfigPath ""$(Get-DisplayPath -PathValue $resolvedConfigPath)"" -VendorDir ""$(Get-DisplayPath -PathValue $resolvedVendorDir)"" -- --port $GatewayPort"

$checkpoints = New-Object System.Collections.Generic.List[object]
$manualCheckpoints = @(
    [ordered]@{
        id = "manual-emergency-stop"
        detail = "人工确认急停按钮、X7 输入与 emergency_stop_active_low=false 的现场极性一致。"
    },
    [ordered]@{
        id = "manual-soft-limit-clearance"
        detail = "人工确认 X/Y 轴当前位置在软限位内，工作区无干涉物。"
    },
    [ordered]@{
        id = "manual-homing-direction"
        detail = "人工确认 Axis1/Axis2 回零方向与 search_distance 不会直接撞向机械硬限位。"
    },
    [ordered]@{
        id = "manual-io-mapping"
        detail = "人工确认 ValveSupply.do_bit_index=2 与 ValveDispenser.cmp_channel=1 / cmp_axis_mask=3 对应现场接线。"
    },
    [ordered]@{
        id = "manual-trigger-smoke"
        detail = "人工确认触发 smoke 在空跑、无产品、无供胶风险条件下执行。"
    }
)

$checkpoints.Add((New-Checkpoint -Id "config-path" -Category "config" -Status $(if ($configExists) { "pass" } else { "fail" }) -Detail $(if ($configExists) { "canonical config exists" } else { "canonical config missing" }) -Evidence (Get-DisplayPath -PathValue $resolvedConfigPath)))
$checkpoints.Add((New-Checkpoint -Id "hardware-mode" -Category "config" -Status $(if ($hardwareMode -eq "Real") { "pass" } else { "fail" }) -Detail $(if ($hardwareMode -eq "Real") { "Hardware.mode=Real" } elseif ([string]::IsNullOrWhiteSpace($hardwareMode)) { "Hardware.mode missing" } else { "Hardware.mode=$hardwareMode" }) -Evidence (Get-DisplayPath -PathValue $resolvedConfigPath)))

$networkFieldsReady = (-not [string]::IsNullOrWhiteSpace($controlCardIp)) -and (-not [string]::IsNullOrWhiteSpace($localIp)) -and (-not [string]::IsNullOrWhiteSpace($networkTimeout))
$checkpoints.Add((New-Checkpoint -Id "network-fields" -Category "config" -Status $(if ($networkFieldsReady) { "pass" } else { "fail" }) -Detail "control_card_ip=$controlCardIp, local_ip=$localIp, timeout_ms=$networkTimeout" -Evidence (Get-DisplayPath -PathValue $resolvedConfigPath)))

$interlockFieldsReady = (-not [string]::IsNullOrWhiteSpace($emergencyStopInput)) -and (-not [string]::IsNullOrWhiteSpace($emergencyStopActiveLow)) -and (-not [string]::IsNullOrWhiteSpace($safetyDoorInput))
$checkpoints.Add((New-Checkpoint -Id "interlock-fields" -Category "config" -Status $(if ($interlockFieldsReady) { "pass" } else { "fail" }) -Detail "emergency_stop_input=$emergencyStopInput, emergency_stop_active_low=$emergencyStopActiveLow, safety_door_input=$safetyDoorInput" -Evidence (Get-DisplayPath -PathValue $resolvedConfigPath)))

$homingFieldsReady = (-not [string]::IsNullOrWhiteSpace($axis1Direction)) -and (-not [string]::IsNullOrWhiteSpace($axis2Direction)) -and (-not [string]::IsNullOrWhiteSpace($axis1SearchDistance)) -and (-not [string]::IsNullOrWhiteSpace($axis2SearchDistance))
$checkpoints.Add((New-Checkpoint -Id "homing-fields" -Category "config" -Status $(if ($homingFieldsReady) { "pass" } else { "fail" }) -Detail "axis1.direction=$axis1Direction, axis2.direction=$axis2Direction, axis1.search_distance=$axis1SearchDistance, axis2.search_distance=$axis2SearchDistance" -Evidence (Get-DisplayPath -PathValue $resolvedConfigPath)))
$checkpoints.Add((New-Checkpoint -Id "velocity-trace-output" -Category "config" -Status $(if (-not [string]::IsNullOrWhiteSpace($velocityTraceOutput)) { "pass" } else { "fail" }) -Detail "VelocityTrace.output_path=$velocityTraceOutput" -Evidence (Get-DisplayPath -PathValue $resolvedConfigPath)))
$checkpoints.Add((New-Checkpoint -Id "debug-log-field" -Category "config" -Status $(if (-not [string]::IsNullOrWhiteSpace($debugLogFile)) { "pass" } else { "fail" }) -Detail "Debug.log_file=$debugLogFile" -Evidence (Get-DisplayPath -PathValue $resolvedConfigPath)))

$checkpoints.Add((New-Checkpoint -Id "vendor-dir" -Category "asset" -Status $(if ($vendorDirExists) { "pass" } else { "fail" }) -Detail $(if ($vendorDirExists) { "vendor directory exists" } else { "vendor directory missing" }) -Evidence (Get-DisplayPath -PathValue $resolvedVendorDir)))
$checkpoints.Add((New-Checkpoint -Id "vendor-dll" -Category "asset" -Status $(if ($vendorDllExists) { "pass" } else { "fail" }) -Detail $(if ($vendorDllExists) { "MultiCard.dll present" } else { "MultiCard.dll missing" }) -Evidence (Get-DisplayPath -PathValue $vendorDllPath)))
$checkpoints.Add((New-Checkpoint -Id "vendor-lib" -Category "asset" -Status $(if ($vendorLibExists) { "pass" } else { "warn" }) -Detail $(if ($vendorLibExists) { "MultiCard.lib present" } else { "MultiCard.lib missing; existing binaries may still dry-run, but rebuild remains blocked" }) -Evidence (Get-DisplayPath -PathValue $vendorLibPath)))

$checkpoints.Add((New-Checkpoint -Id "runtime-service-script" -Category "owner-boundary" -Status $(if (Test-Path $runtimeServiceScript) { "pass" } else { "fail" }) -Detail $(if (Test-Path $runtimeServiceScript) { "runtime-service script present" } else { "runtime-service script missing" }) -Evidence (Get-DisplayPath -PathValue $runtimeServiceScript)))
$checkpoints.Add((New-Checkpoint -Id "runtime-gateway-script" -Category "owner-boundary" -Status $(if (Test-Path $runtimeGatewayScript) { "pass" } else { "fail" }) -Detail $(if (Test-Path $runtimeGatewayScript) { "runtime-gateway script present" } else { "runtime-gateway script missing" }) -Evidence (Get-DisplayPath -PathValue $runtimeGatewayScript)))
$checkpoints.Add((New-Checkpoint -Id "runtime-service-exe" -Category "owner-boundary" -Status $(if (-not [string]::IsNullOrWhiteSpace($runtimeServiceExe)) { "pass" } else { "fail" }) -Detail $(if (-not [string]::IsNullOrWhiteSpace($runtimeServiceExe)) { "runtime-service executable resolved" } else { "runtime-service executable missing under control-apps build root" }) -Evidence (Get-DisplayPath -PathValue $runtimeServiceExe)))
$checkpoints.Add((New-Checkpoint -Id "runtime-gateway-exe" -Category "owner-boundary" -Status $(if (-not [string]::IsNullOrWhiteSpace($runtimeGatewayExe)) { "pass" } else { "fail" }) -Detail $(if (-not [string]::IsNullOrWhiteSpace($runtimeGatewayExe)) { "runtime-gateway executable resolved" } else { "runtime-gateway executable missing under control-apps build root" }) -Evidence (Get-DisplayPath -PathValue $runtimeGatewayExe)))

$serviceDryRun = Invoke-AppDryRunCheck `
    -ScriptPath $runtimeServiceScript `
    -BaseArguments @("-BuildConfig", $BuildConfig, "-ConfigPath", $resolvedConfigPath, "-VendorDir", $resolvedVendorDir) `
    -StrictLogPath (Join-Path $logsDir "runtime-service-dryrun-strict.log") `
    -DiagnosticLogPath (Join-Path $logsDir "runtime-service-dryrun-diagnostic.log")

$gatewayDryRun = Invoke-AppDryRunCheck `
    -ScriptPath $runtimeGatewayScript `
    -BaseArguments @("-BuildConfig", $BuildConfig, "-ConfigPath", $resolvedConfigPath, "-VendorDir", $resolvedVendorDir) `
    -StrictLogPath (Join-Path $logsDir "runtime-gateway-dryrun-strict.log") `
    -DiagnosticLogPath (Join-Path $logsDir "runtime-gateway-dryrun-diagnostic.log")

$checkpoints.Add((New-Checkpoint -Id "runtime-service-dryrun-strict" -Category $(if ($serviceDryRun.classification -eq "none") { "owner-boundary" } else { $serviceDryRun.classification }) -Status $(if ($serviceDryRun.strict.exit_code -eq 0) { "pass" } else { "fail" }) -Detail $serviceDryRun.strict.output -Evidence (Get-DisplayPath -PathValue (Join-Path $logsDir "runtime-service-dryrun-strict.log"))))
$checkpoints.Add((New-Checkpoint -Id "runtime-service-dryrun-diagnostic" -Category "owner-boundary" -Status $(if ($serviceDryRun.diagnostic.exit_code -eq 0) { "pass" } else { "fail" }) -Detail $serviceDryRun.diagnostic.output -Evidence (Get-DisplayPath -PathValue (Join-Path $logsDir "runtime-service-dryrun-diagnostic.log"))))
$checkpoints.Add((New-Checkpoint -Id "runtime-gateway-dryrun-strict" -Category $(if ($gatewayDryRun.classification -eq "none") { "owner-boundary" } else { $gatewayDryRun.classification }) -Status $(if ($gatewayDryRun.strict.exit_code -eq 0) { "pass" } else { "fail" }) -Detail $gatewayDryRun.strict.output -Evidence (Get-DisplayPath -PathValue (Join-Path $logsDir "runtime-gateway-dryrun-strict.log"))))
$checkpoints.Add((New-Checkpoint -Id "runtime-gateway-dryrun-diagnostic" -Category "owner-boundary" -Status $(if ($gatewayDryRun.diagnostic.exit_code -eq 0) { "pass" } else { "fail" }) -Detail $gatewayDryRun.diagnostic.output -Evidence (Get-DisplayPath -PathValue (Join-Path $logsDir "runtime-gateway-dryrun-diagnostic.log"))))

$configFailures = @($checkpoints | Where-Object { ($_.category -eq "config") -and ($_.status -eq "fail") })
$assetFailures = @($checkpoints | Where-Object { ($_.category -eq "asset") -and ($_.status -eq "fail") })
$boundaryFailures = @($checkpoints | Where-Object { ($_.category -eq "owner-boundary") -and ($_.status -eq "fail") })

$observationResult = "ready_for_gate_review"
$a4Signal = "NeedsManualReview"
$dominantGap = "none"
$nextAction = "自动检查已通过；继续按 docs/runtime/hardware-bring-up-smoke-sop.md 执行人工安全检查，再交 A4 评审。"

if ($configFailures.Count -gt 0) {
    $observationResult = "blocked"
    $a4Signal = "No-Go"
    $dominantGap = "configuration"
    $nextAction = "修正 canonical machine config 路径或关键字段后，重新执行 run-hardware-smoke-observation.ps1。"
} elseif ($assetFailures.Count -gt 0) {
    $observationResult = "blocked"
    $a4Signal = "No-Go"
    $dominantGap = "vendor-assets"
    $nextAction = "补齐 MultiCard.dll 或修正 VendorDir 覆盖目录后，重新执行 run-hardware-smoke-observation.ps1。"
} elseif ($boundaryFailures.Count -gt 0) {
    $observationResult = "blocked"
    $a4Signal = "No-Go"
    $dominantGap = "owner-boundary"
    $nextAction = "修复 canonical app 脚本、构建产物或 dry-run 诊断失败项后，重新执行 run-hardware-smoke-observation.ps1。"
}

$automaticCheckpointArray = $checkpoints.ToArray()
$manualCheckpointArray = @($manualCheckpoints)

$summaryPayload = [ordered]@{
    generated_at = (Get-Date).ToString("s")
    report_dir = $runDir
    workspace_root = $workspaceRoot
    build_config = $BuildConfig
    machine_id = $MachineId
    config_path = $resolvedConfigPath
    vendor_dir = $resolvedVendorDir
    control_apps_build_root = $controlAppsBuildRoot
    control_apps_build_roots = @($controlAppsBuildRoots)
    log_locations = [ordered]@{
        runtime_service_default = "logs/control_runtime.log"
        runtime_gateway_default = "logs/tcp_server.log"
        config_debug_log = $debugLogFile
        velocity_trace_output = $velocityTraceOutput
    }
    launch_commands = [ordered]@{
        runtime_service = $runtimeServiceCommandText
        runtime_gateway = $runtimeGatewayCommandText
    }
    summary = [ordered]@{
        observation_result = $observationResult
        a4_signal = $a4Signal
        dominant_gap = $dominantGap
        next_action = $nextAction
    }
    automatic_checkpoints = $automaticCheckpointArray
    manual_checkpoints = $manualCheckpointArray
}

$summaryJsonPath = Join-Path $runDir "hardware-smoke-observation-summary.json"
$summaryMdPath = Join-Path $runDir "hardware-smoke-observation-summary.md"
$summaryPayload | ConvertTo-Json -Depth 8 | Set-Content -Path $summaryJsonPath -Encoding utf8

$mdLines = @()
$mdLines += "# Hardware Smoke Observation Summary"
$mdLines += ""
$mdLines += ('- generated_at: `{0}`' -f $summaryPayload.generated_at)
$mdLines += ('- report_dir: `{0}`' -f (Get-DisplayPath -PathValue $runDir))
$mdLines += ('- build_config: `{0}`' -f $BuildConfig)
$mdLines += ('- machine_id: `{0}`' -f $MachineId)
$mdLines += ('- config_path: `{0}`' -f (Get-DisplayPath -PathValue $resolvedConfigPath))
$mdLines += ('- vendor_dir: `{0}`' -f (Get-DisplayPath -PathValue $resolvedVendorDir))
$mdLines += ""
$mdLines += "## Result Summary"
$mdLines += ""
$mdLines += '```text'
$mdLines += ('observation_result = {0}' -f $observationResult)
$mdLines += ('a4_signal = {0}' -f $a4Signal)
$mdLines += ('dominant_gap = {0}' -f $dominantGap)
$mdLines += ('config_path = {0}' -f (Get-DisplayPath -PathValue $resolvedConfigPath))
$mdLines += ('runtime_service_command = {0}' -f $runtimeServiceCommandText)
$mdLines += ('runtime_gateway_command = {0}' -f $runtimeGatewayCommandText)
$mdLines += ('runtime_service_log = {0}' -f $summaryPayload.log_locations.runtime_service_default)
$mdLines += ('runtime_gateway_log = {0}' -f $summaryPayload.log_locations.runtime_gateway_default)
$mdLines += ('velocity_trace_output = {0}' -f $summaryPayload.log_locations.velocity_trace_output)
$mdLines += ('next_action = {0}' -f $nextAction)
$mdLines += '```'
$mdLines += ""
$mdLines += "## Launch Commands"
$mdLines += ""
$mdLines += '```powershell'
$mdLines += $runtimeServiceCommandText
$mdLines += $runtimeGatewayCommandText
$mdLines += '```'
$mdLines += ""
$mdLines += "## Automatic Checkpoints"
$mdLines += ""
$mdLines += "| id | category | status | evidence | detail |"
$mdLines += "|---|---|---|---|---|"
foreach ($checkpoint in $checkpoints) {
    $detail = [string]$checkpoint.detail
    $detail = $detail -replace '\r?\n', ' '
    if ($detail.Length -gt 220) {
        $detail = $detail.Substring(0, 220) + "..."
    }
    $mdLines += ('| {0} | `{1}` | `{2}` | `{3}` | {4} |' -f $checkpoint.id, $checkpoint.category, $checkpoint.status, $checkpoint.evidence, $detail)
}
$mdLines += ""
$mdLines += "## Manual Checkpoints"
$mdLines += ""
foreach ($manualCheckpoint in $manualCheckpoints) {
    $mdLines += ('- `{0}`: {1}' -f $manualCheckpoint.id, $manualCheckpoint.detail)
}
$mdLines += ""
$mdLines += "## Machine-Readable Sidecar"
$mdLines += ""
$mdLines += ('- json: `{0}`' -f (Get-DisplayPath -PathValue $summaryJsonPath))

$mdLines | Set-Content -Path $summaryMdPath -Encoding utf8

$registerLogPath = $null
if ($RegisterObservation) {
    $registerLogPath = Join-Path $runDir "register-hardware-smoke-observation.log"
    $registerArguments = @(
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        $registerScript,
        "-SummaryPath",
        $summaryJsonPath,
        "-EvidenceDir",
        $runDir,
        "-MachineId",
        $MachineId
    )

    if (-not [string]::IsNullOrWhiteSpace($Operator)) {
        $registerArguments += @("-Operator", $Operator)
    }

    $registerResult = Invoke-ExternalProcessCapture -FilePath "powershell" -Arguments $registerArguments
    $registerResult.output | Set-Content -Path $registerLogPath -Encoding utf8
}

Write-Output "hardware smoke observation complete"
Write-Output ("report dir: {0}" -f $runDir)
Write-Output ("summary markdown: {0}" -f $summaryMdPath)
Write-Output ("summary json: {0}" -f $summaryJsonPath)
Write-Output ("config path: {0}" -f $resolvedConfigPath)
Write-Output ("runtime-service command: {0}" -f $runtimeServiceCommandText)
Write-Output ("runtime-gateway command: {0}" -f $runtimeGatewayCommandText)
Write-Output ("runtime-service log: logs/control_runtime.log")
Write-Output ("runtime-gateway log: logs/tcp_server.log")
Write-Output ("observation_result: {0}" -f $observationResult)
Write-Output ("a4_signal: {0}" -f $a4Signal)
Write-Output ("dominant_gap: {0}" -f $dominantGap)
if ($registerLogPath) {
    Write-Output ("register log: {0}" -f $registerLogPath)
}

if ($observationResult -eq "blocked") {
    exit 1
}

exit 0
