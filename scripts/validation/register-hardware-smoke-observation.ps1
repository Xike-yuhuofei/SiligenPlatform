[CmdletBinding()]
param(
    [string]$SummaryPath,
    [string]$EvidenceDir,
    [string]$ReportDir = "tests/reports/hardware-smoke-observation/intake",
    [string]$Operator,
    [string]$MachineId = "unknown-machine",
    [ValidateSet("Auto", "ReadyForGateReview", "Blocked", "Pending")]
    [string]$Status = "Auto",
    [string]$Notes
)

$ErrorActionPreference = "Stop"

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent

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

function Get-SafeName {
    param([string]$Value)

    $safe = $Value -replace '[^A-Za-z0-9\-]+', '-'
    $safe = $safe.Trim('-')
    if ([string]::IsNullOrWhiteSpace($safe)) {
        return "unknown-machine"
    }

    return $safe.ToLowerInvariant()
}

$resolvedEvidenceDir = $null
if (-not [string]::IsNullOrWhiteSpace($EvidenceDir)) {
    $resolvedEvidenceDir = Resolve-FullPath -PathValue $EvidenceDir
}

if ([string]::IsNullOrWhiteSpace($SummaryPath) -and -not [string]::IsNullOrWhiteSpace($resolvedEvidenceDir)) {
    $SummaryPath = Join-Path $resolvedEvidenceDir "hardware-smoke-observation-summary.json"
}

$resolvedSummaryPath = $null
if (-not [string]::IsNullOrWhiteSpace($SummaryPath)) {
    $resolvedSummaryPath = Resolve-FullPath -PathValue $SummaryPath
}

$summaryPayload = $null
if (-not [string]::IsNullOrWhiteSpace($resolvedSummaryPath) -and (Test-Path $resolvedSummaryPath)) {
    $summaryPayload = Get-Content -Raw $resolvedSummaryPath | ConvertFrom-Json
}

if ($null -eq $summaryPayload -and [string]::IsNullOrWhiteSpace($resolvedEvidenceDir)) {
    throw "必须至少提供 EvidenceDir 或可读取的 SummaryPath。"
}

if ($null -eq $summaryPayload -and -not [string]::IsNullOrWhiteSpace($resolvedSummaryPath)) {
    throw "summary json not found: $resolvedSummaryPath"
}

if ([string]::IsNullOrWhiteSpace($Operator)) {
    $Operator = if (-not [string]::IsNullOrWhiteSpace($env:USERNAME)) { $env:USERNAME } else { "unknown-operator" }
}

if ([string]::IsNullOrWhiteSpace($resolvedEvidenceDir) -and $null -ne $summaryPayload) {
    $resolvedEvidenceDir = [string]$summaryPayload.report_dir
}

$resolvedStatus = switch ($Status) {
    "Auto" {
        if ($null -eq $summaryPayload) {
            "pending"
        } elseif ($summaryPayload.summary.observation_result -eq "ready_for_gate_review") {
            "ready_for_gate_review"
        } else {
            "blocked"
        }
    }
    "ReadyForGateReview" { "ready_for_gate_review" }
    "Blocked" { "blocked" }
    "Pending" { "pending" }
    default { throw "unsupported status: $Status" }
}

$resolvedReportDir = Resolve-FullPath -PathValue $ReportDir
New-Item -ItemType Directory -Force -Path $resolvedReportDir | Out-Null

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$baseName = "{0}-{1}" -f $timestamp, (Get-SafeName -Value $MachineId)
$jsonPath = Join-Path $resolvedReportDir ($baseName + ".json")
$mdPath = Join-Path $resolvedReportDir ($baseName + ".md")

$payload = [ordered]@{
    collected_at = (Get-Date).ToString("o")
    operator = $Operator
    machine_id = $MachineId
    status = $resolvedStatus
    summary_path = $resolvedSummaryPath
    evidence_dir = $resolvedEvidenceDir
    observation_result = $(if ($null -ne $summaryPayload) { [string]$summaryPayload.summary.observation_result } else { "pending" })
    a4_signal = $(if ($null -ne $summaryPayload) { [string]$summaryPayload.summary.a4_signal } else { "NeedsManualReview" })
    dominant_gap = $(if ($null -ne $summaryPayload) { [string]$summaryPayload.summary.dominant_gap } else { "unknown" })
    config_path = $(if ($null -ne $summaryPayload) { [string]$summaryPayload.config_path } else { $null })
    vendor_dir = $(if ($null -ne $summaryPayload) { [string]$summaryPayload.vendor_dir } else { $null })
    runtime_service_command = $(if ($null -ne $summaryPayload) { [string]$summaryPayload.launch_commands.runtime_service } else { $null })
    runtime_gateway_command = $(if ($null -ne $summaryPayload) { [string]$summaryPayload.launch_commands.runtime_gateway } else { $null })
    runtime_service_log = $(if ($null -ne $summaryPayload) { [string]$summaryPayload.log_locations.runtime_service_default } else { "logs/control_runtime.log" })
    runtime_gateway_log = $(if ($null -ne $summaryPayload) { [string]$summaryPayload.log_locations.runtime_gateway_default } else { "logs/tcp_server.log" })
    velocity_trace_output = $(if ($null -ne $summaryPayload) { [string]$summaryPayload.log_locations.velocity_trace_output } else { $null })
    next_action = $(if ($null -ne $summaryPayload) { [string]$summaryPayload.summary.next_action } else { "先运行 run-hardware-smoke-observation.ps1 生成 summary，再重新登记。" })
    notes = $Notes
}

$payload | ConvertTo-Json -Depth 6 | Set-Content -Path $jsonPath -Encoding utf8

$mdLines = @()
$mdLines += "# Hardware Smoke Observation Intake"
$mdLines += ""
$mdLines += ('- collected_at: `{0}`' -f $payload.collected_at)
$mdLines += ('- operator: `{0}`' -f $payload.operator)
$mdLines += ('- machine_id: `{0}`' -f $payload.machine_id)
$mdLines += ('- status: `{0}`' -f $payload.status)
$mdLines += ('- observation_result: `{0}`' -f $payload.observation_result)
$mdLines += ('- a4_signal: `{0}`' -f $payload.a4_signal)
$mdLines += ('- dominant_gap: `{0}`' -f $payload.dominant_gap)
$mdLines += ('- summary_path: `{0}`' -f (Get-DisplayPath -PathValue $payload.summary_path))
$mdLines += ('- evidence_dir: `{0}`' -f (Get-DisplayPath -PathValue $payload.evidence_dir))
$mdLines += ('- config_path: `{0}`' -f (Get-DisplayPath -PathValue $payload.config_path))
$mdLines += ('- vendor_dir: `{0}`' -f (Get-DisplayPath -PathValue $payload.vendor_dir))
$mdLines += ('- runtime_service_command: `{0}`' -f $payload.runtime_service_command)
$mdLines += ('- runtime_gateway_command: `{0}`' -f $payload.runtime_gateway_command)
$mdLines += ('- runtime_service_log: `{0}`' -f $payload.runtime_service_log)
$mdLines += ('- runtime_gateway_log: `{0}`' -f $payload.runtime_gateway_log)
$mdLines += ('- velocity_trace_output: `{0}`' -f $payload.velocity_trace_output)
$mdLines += ('- next_action: `{0}`' -f $payload.next_action)
if (-not [string]::IsNullOrWhiteSpace($Notes)) {
    $mdLines += ('- notes: `{0}`' -f $Notes)
}
$mdLines += ""
$mdLines += "## Machine-Readable Sidecar"
$mdLines += ""
$mdLines += ('- json: `{0}`' -f (Get-DisplayPath -PathValue $jsonPath))

$mdLines | Set-Content -Path $mdPath -Encoding utf8

Write-Output "hardware smoke observation intake registered"
Write-Output ("markdown: {0}" -f $mdPath)
Write-Output ("json: {0}" -f $jsonPath)
