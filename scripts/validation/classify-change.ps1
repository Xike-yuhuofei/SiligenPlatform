[CmdletBinding()]
param(
    [ValidateSet("native", "hil", "all")]
    [string]$Mode = "all",
    [string]$OutputPath = "tests\reports\change-classification\classification.json",
    [string]$BaseSha = "",
    [string]$HeadSha = "",
    [string[]]$ChangedFile = @(),
    [string[]]$Label = @(),
    [switch]$ForceNative,
    [switch]$ForceHil,
    [string]$EventName = $env:GITHUB_EVENT_NAME
)

$ErrorActionPreference = "Stop"

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$configPath = Join-Path $PSScriptRoot "gates\change-classification.json"

function Resolve-WorkspacePath {
    param([string]$PathValue)

    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return [System.IO.Path]::GetFullPath($PathValue)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $workspaceRoot $PathValue))
}

function ConvertTo-StringArray {
    param($Value)

    if ($null -eq $Value) {
        return @()
    }
    if ($Value -is [System.Array]) {
        return @($Value | ForEach-Object { [string]$_ })
    }
    return @([string]$Value)
}

function Expand-CommaSeparatedValues {
    param([string[]]$Values)

    $expanded = @()
    foreach ($value in @($Values)) {
        if ([string]::IsNullOrWhiteSpace($value)) {
            continue
        }
        $expanded += @(([string]$value).Split(",") | ForEach-Object { $_.Trim() } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    }
    return @($expanded)
}

function Get-JsonPropertyValue {
    param(
        [Parameter(Mandatory = $true)]$Object,
        [Parameter(Mandatory = $true)][string]$Name
    )

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        return $null
    }
    return $property.Value
}

function Get-GitHubEvent {
    if ([string]::IsNullOrWhiteSpace($env:GITHUB_EVENT_PATH)) {
        return $null
    }
    if (-not (Test-Path -LiteralPath $env:GITHUB_EVENT_PATH)) {
        return $null
    }
    return Get-Content -Raw -Path $env:GITHUB_EVENT_PATH | ConvertFrom-Json
}

function Get-EventLabels {
    param($Event)

    if ($null -eq $Event -or $null -eq $Event.pull_request -or $null -eq $Event.pull_request.labels) {
        return @()
    }
    return @($Event.pull_request.labels | ForEach-Object { [string]$_.name } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
}

function Invoke-ChangedFileCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CommandName,
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    $output = @(& $CommandName @Arguments 2>&1)
    $exitCode = if ($null -ne $LASTEXITCODE) { [int]$LASTEXITCODE } else { 0 }
    if ($exitCode -ne 0) {
        $message = ($output | ForEach-Object { [string]$_ }) -join "`n"
        throw "$CommandName $($Arguments -join ' ') failed with exit code ${exitCode}: $message"
    }
    return @($output | ForEach-Object { [string]$_ })
}

function Get-ChangedFiles {
    param(
        $Event,
        [string]$Base,
        [string]$Head,
        [string]$EventNameValue
    )

    $result = [ordered]@{
        reliable = $true
        files = @()
        source = ""
        error = ""
    }

    try {
        if ($null -ne $Event -and $EventNameValue -eq "pull_request" -and $null -ne $Event.pull_request) {
            $prNumber = [string]$Event.pull_request.number
            if (-not [string]::IsNullOrWhiteSpace($prNumber) -and -not [string]::IsNullOrWhiteSpace($env:GITHUB_REPOSITORY)) {
                $result.files = @(Invoke-ChangedFileCommand -CommandName "gh" -Arguments @("pr", "diff", $prNumber, "--repo", $env:GITHUB_REPOSITORY, "--name-only"))
                $result.source = "gh-pr-diff"
                return [pscustomobject]$result
            }
        }

        if (-not [string]::IsNullOrWhiteSpace($Base) -and -not [string]::IsNullOrWhiteSpace($Head)) {
            $result.files = @(Invoke-ChangedFileCommand -CommandName "git" -Arguments @("diff", "--name-only", $Base, $Head))
            $result.source = "git-diff-base-head"
            return [pscustomobject]$result
        }

        $result.files = @(Invoke-ChangedFileCommand -CommandName "git" -Arguments @("diff", "--name-only", "HEAD~1", "HEAD"))
        $result.source = "git-diff-head"
        return [pscustomobject]$result
    } catch {
        $result.reliable = $false
        $result.error = $_.Exception.Message
        $result.files = @()
        $result.source = "unavailable"
        return [pscustomobject]$result
    }
}

function Test-Truthy {
    param(
        [bool]$SwitchPresent,
        [string]$EnvironmentValue
    )

    if ($SwitchPresent) {
        return $true
    }
    return ($EnvironmentValue -match '^(?i:true|1|yes)$')
}

function Get-RuleEvaluation {
    param(
        [string]$RuleName,
        $Rule,
        [string[]]$Files,
        [string[]]$Labels,
        [bool]$Force,
        [bool]$ClassificationReliable
    )

    $requires = $false
    $reasons = New-Object System.Collections.Generic.List[string]
    $patterns = ConvertTo-StringArray -Value $Rule.patterns
    $ruleLabels = ConvertTo-StringArray -Value $Rule.labels

    if ($Force) {
        $requires = $true
        $reasons.Add("workflow_dispatch $($Rule.forceInput)=true")
    }

    foreach ($label in $ruleLabels) {
        if ($Labels -contains $label) {
            $requires = $true
            $reasons.Add("label $label")
        }
    }

    if (-not $ClassificationReliable -and $Rule.defaultRequiredOnClassificationFailure -eq $true) {
        $requires = $true
        $reasons.Add("changed-file classification unavailable; default $RuleName-required")
    }

    foreach ($file in @($Files)) {
        $normalized = ([string]$file) -replace '\\', '/'
        foreach ($pattern in $patterns) {
            if ($normalized -match $pattern) {
                $requires = $true
                $reasons.Add("path $normalized matches $pattern")
                break
            }
        }
    }

    $uniqueReasons = @($reasons | Select-Object -Unique)
    if ($uniqueReasons.Count -eq 0) {
        $uniqueReasons = @("not $RuleName-sensitive")
    }

    return [pscustomobject]@{
        requires = $requires
        reasons = @($uniqueReasons)
        reason_text = ($uniqueReasons -join "; ")
    }
}

function Write-GitHubOutput {
    param(
        [string]$ModeValue,
        [bool]$RequiresNative,
        [bool]$RequiresHil,
        [string]$NativeReason,
        [string]$HilReason
    )

    if ([string]::IsNullOrWhiteSpace($env:GITHUB_OUTPUT)) {
        return
    }

    $requiresNativeText = if ($RequiresNative) { "true" } else { "false" }
    $requiresHilText = if ($RequiresHil) { "true" } else { "false" }
    Add-Content -Path $env:GITHUB_OUTPUT -Value "requires_native=$requiresNativeText"
    Add-Content -Path $env:GITHUB_OUTPUT -Value "requires_hil=$requiresHilText"
    Add-Content -Path $env:GITHUB_OUTPUT -Value "native_reason=$NativeReason"
    Add-Content -Path $env:GITHUB_OUTPUT -Value "hil_reason=$HilReason"

    if ($ModeValue -eq "native") {
        Add-Content -Path $env:GITHUB_OUTPUT -Value "reason=$NativeReason"
    } elseif ($ModeValue -eq "hil") {
        Add-Content -Path $env:GITHUB_OUTPUT -Value "reason=$HilReason"
    }
}

function Write-GitHubSummary {
    param(
        [hashtable]$Summary
    )

    if ([string]::IsNullOrWhiteSpace($env:GITHUB_STEP_SUMMARY)) {
        return
    }

    Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "## Change classification"
    Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "- mode: ``$($Summary.mode)``"
    Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "- requires_native: ``$($Summary.requires_native)``"
    Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "- native_reason: ``$($Summary.native_reason)``"
    Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "- requires_hil: ``$($Summary.requires_hil)``"
    Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "- hil_reason: ``$($Summary.hil_reason)``"
    Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "- classification_reliable: ``$($Summary.classification_reliable)``"
    Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value ""
    Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "### Changed files"
    foreach ($file in @($Summary.changed_files)) {
        Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "- ``$file``"
    }
}

if (-not (Test-Path -LiteralPath $configPath)) {
    throw "Change classification configuration not found: $configPath"
}

$config = Get-Content -Raw -Path $configPath | ConvertFrom-Json
$event = Get-GitHubEvent

$Label = Expand-CommaSeparatedValues -Values $Label
$ChangedFile = Expand-CommaSeparatedValues -Values $ChangedFile

$labels = @($Label | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
if ($labels.Count -eq 0) {
    $labels = Get-EventLabels -Event $event
}

$changedFiles = @($ChangedFile | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
$fileSource = "parameter"
$classificationReliable = $true
$classificationError = ""
if ($changedFiles.Count -eq 0) {
    $gitFiles = Get-ChangedFiles -Event $event -Base $BaseSha -Head $HeadSha -EventNameValue $EventName
    $changedFiles = @($gitFiles.files)
    $fileSource = $gitFiles.source
    $classificationReliable = [bool]$gitFiles.reliable
    $classificationError = [string]$gitFiles.error
}

$forceNativeValue = Test-Truthy -SwitchPresent $ForceNative.IsPresent -EnvironmentValue $env:FORCE_NATIVE
$forceHilValue = Test-Truthy -SwitchPresent $ForceHil.IsPresent -EnvironmentValue $env:FORCE_HIL

$nativeRule = Get-JsonPropertyValue -Object $config -Name "native"
$hilRule = Get-JsonPropertyValue -Object $config -Name "hil"
if ($null -eq $nativeRule -or $null -eq $hilRule) {
    throw "change-classification.json must define native and hil rules."
}

$native = Get-RuleEvaluation `
    -RuleName "native" `
    -Rule $nativeRule `
    -Files $changedFiles `
    -Labels $labels `
    -Force $forceNativeValue `
    -ClassificationReliable $classificationReliable
$hil = Get-RuleEvaluation `
    -RuleName "hardware" `
    -Rule $hilRule `
    -Files $changedFiles `
    -Labels $labels `
    -Force $forceHilValue `
    -ClassificationReliable $classificationReliable

$output = [ordered]@{
    schemaVersion = 1
    generated_at = (Get-Date).ToString("o")
    mode = $Mode
    base_sha = $BaseSha
    head_sha = $HeadSha
    event_name = $EventName
    changed_file_source = $fileSource
    classification_reliable = $classificationReliable
    classification_error = $classificationError
    changed_files = @($changedFiles)
    labels = @($labels)
    requires_native = [bool]$native.requires
    requires_hil = [bool]$hil.requires
    native_reason = [string]$native.reason_text
    hil_reason = [string]$hil.reason_text
    reasons = [ordered]@{
        native = @($native.reasons)
        hil = @($hil.reasons)
    }
}

$resolvedOutputPath = Resolve-WorkspacePath -PathValue $OutputPath
$outputDir = Split-Path -Path $resolvedOutputPath -Parent
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$output | ConvertTo-Json -Depth 8 | Set-Content -Path $resolvedOutputPath -Encoding UTF8

Write-GitHubOutput `
    -ModeValue $Mode `
    -RequiresNative ([bool]$native.requires) `
    -RequiresHil ([bool]$hil.requires) `
    -NativeReason ([string]$native.reason_text) `
    -HilReason ([string]$hil.reason_text)
Write-GitHubSummary -Summary $output

Write-Output "classification=$resolvedOutputPath"
Write-Output "requires_native=$($output.requires_native) reason=$($output.native_reason)"
Write-Output "requires_hil=$($output.requires_hil) reason=$($output.hil_reason)"
