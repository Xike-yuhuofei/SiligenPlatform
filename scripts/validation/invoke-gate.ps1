[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("pre-push", "pr", "native", "hil", "release", "full-offline", "nightly")]
    [string]$Gate,

    [string]$ReportRoot = "",
    [string[]]$ChangedScope = @(),
    [string[]]$SelectedStep = @(),
    [string[]]$SkipStep = @(),
    [string]$SkipJustification = "",
    [string]$ClassificationPath = "",
    [string]$BaseSha = "",
    [string]$HeadSha = "",
    [switch]$DirtyWorktree,
    [string]$DirtyPolicy = "",
    [switch]$ForceNative,
    [switch]$ForceHil,

    [ValidateSet("all", "apps", "contracts", "protocol-compatibility", "integration", "e2e", "performance")]
    [string[]]$Suite = @("all"),
    [ValidateSet("auto", "quick-gate", "full-offline-gate", "nightly-performance", "limited-hil")]
    [string]$Lane = "auto",
    [ValidateSet("low", "medium", "high", "hardware-sensitive")]
    [string]$RiskProfile = "medium",
    [ValidateSet("auto", "quick", "full-offline", "nightly", "hil")]
    [string]$DesiredDepth = "auto",
    [string[]]$SkipLayer = @(),
    [switch]$IncludeHardwareSmoke,
    [switch]$IncludeHilClosedLoop,
    [switch]$IncludeHilCaseMatrix,
    [switch]$EnablePythonCoverage,
    [switch]$EnableCppCoverage
)

$ErrorActionPreference = "Stop"

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$configPath = Join-Path $PSScriptRoot "gates\gates.json"

function Resolve-WorkspacePath {
    param([string]$PathValue)

    if ([string]::IsNullOrWhiteSpace($PathValue)) {
        return ""
    }
    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return [System.IO.Path]::GetFullPath($PathValue)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $workspaceRoot $PathValue))
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

function Get-SwitchArgument {
    param(
        [string]$Name,
        [bool]$Enabled
    )

    if ($Enabled) {
        return @($Name)
    }
    return @()
}

function Get-NamedValueArguments {
    param(
        [string]$Name,
        [string[]]$Values
    )

    $items = @($Values | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    if ($items.Count -eq 0) {
        return @()
    }
    return @($Name) + $items
}

function Get-NamedScalarArgument {
    param(
        [string]$Name,
        [string]$Value
    )

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return @()
    }
    return @($Name, $Value)
}

function Get-DefaultGateValue {
    param(
        [string]$Name,
        [string]$GateName
    )

    switch ($Name) {
        "Lane" {
            if ($GateName -in @("full-offline", "native")) {
                return "full-offline-gate"
            }
            if ($GateName -eq "nightly") {
                return "nightly-performance"
            }
            return "auto"
        }
        "RiskProfile" {
            if ($GateName -in @("full-offline", "native", "nightly")) {
                return "high"
            }
            if ($GateName -eq "hil") {
                return "hardware-sensitive"
            }
            return "medium"
        }
        "DesiredDepth" {
            if ($GateName -in @("full-offline", "native")) {
                return "full-offline"
            }
            if ($GateName -eq "nightly") {
                return "nightly"
            }
            if ($GateName -eq "hil") {
                return "hil"
            }
            return "auto"
        }
    }
}

function Expand-TemplateValue {
    param(
        [string]$Value,
        [hashtable]$Context
    )

    $expanded = $Value
    foreach ($key in $Context.Keys) {
        $expanded = $expanded.Replace("{$key}", [string]$Context[$key])
    }
    return $expanded
}

function Expand-CommandTemplate {
    param(
        [object[]]$Command,
        [hashtable]$Context,
        [hashtable]$Runtime
    )

    $expanded = @()
    foreach ($item in @($Command)) {
        $token = [string]$item
        switch ($token) {
            "{suiteArgs}" {
                $expanded += Get-NamedValueArguments -Name "-Suite" -Values $Runtime.Suite
                continue
            }
            "{laneArgs}" {
                $expanded += Get-NamedScalarArgument -Name "-Lane" -Value $Runtime.Lane
                continue
            }
            "{riskProfileArgs}" {
                $expanded += Get-NamedScalarArgument -Name "-RiskProfile" -Value $Runtime.RiskProfile
                continue
            }
            "{desiredDepthArgs}" {
                $expanded += Get-NamedScalarArgument -Name "-DesiredDepth" -Value $Runtime.DesiredDepth
                continue
            }
            "{changedScopeArgs}" {
                $expanded += Get-NamedValueArguments -Name "-ChangedScope" -Values $Runtime.ChangedScope
                continue
            }
            "{changedFileArgs}" {
                $changedFiles = @($Runtime.ChangedScope | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
                if ($changedFiles.Count -gt 0) {
                    $expanded += @("-ChangedFile", ($changedFiles -join ","))
                }
                continue
            }
            "{classificationPathArgs}" {
                $expanded += Get-NamedScalarArgument -Name "-ClassificationPath" -Value $Runtime.ClassificationPath
                continue
            }
            "{baseShaArgs}" {
                $expanded += Get-NamedScalarArgument -Name "-BaseSha" -Value $Runtime.BaseSha
                continue
            }
            "{headShaArgs}" {
                $expanded += Get-NamedScalarArgument -Name "-HeadSha" -Value $Runtime.HeadSha
                continue
            }
            "{dirtyWorktreeArgs}" {
                $expanded += Get-SwitchArgument -Name "-DirtyWorktree" -Enabled $Runtime.DirtyWorktree
                $expanded += Get-NamedScalarArgument -Name "-DirtyPolicy" -Value $Runtime.DirtyPolicy
                continue
            }
            "{skipLayerArgs}" {
                $expanded += Get-NamedValueArguments -Name "-SkipLayer" -Values $Runtime.SkipLayer
                if (-not [string]::IsNullOrWhiteSpace($Runtime.SkipJustification)) {
                    $expanded += @("-SkipJustification", $Runtime.SkipJustification)
                }
                continue
            }
            "{includeHilArgs}" {
                $expanded += Get-SwitchArgument -Name "-IncludeHardwareSmoke" -Enabled $Runtime.IncludeHardwareSmoke
                $expanded += Get-SwitchArgument -Name "-IncludeHilClosedLoop" -Enabled $Runtime.IncludeHilClosedLoop
                $expanded += Get-SwitchArgument -Name "-IncludeHilCaseMatrix" -Enabled $Runtime.IncludeHilCaseMatrix
                continue
            }
            "{pythonCoverageArgs}" {
                $expanded += Get-SwitchArgument -Name "-EnablePythonCoverage" -Enabled $Runtime.EnablePythonCoverage
                continue
            }
            "{cppCoverageArgs}" {
                $expanded += Get-SwitchArgument -Name "-EnableCppCoverage" -Enabled $Runtime.EnableCppCoverage
                continue
            }
            default {
                $value = Expand-TemplateValue -Value $token -Context $Context
                if (-not [string]::IsNullOrWhiteSpace($value)) {
                    $expanded += $value
                }
            }
        }
    }
    return @($expanded)
}

function ConvertTo-ProcessArgumentLine {
    param([string[]]$Arguments)

    $quoted = @()
    foreach ($arg in @($Arguments)) {
        if ($null -eq $arg) {
            continue
        }

        $text = [string]$arg
        if ($text.Length -eq 0) {
            $quoted += '""'
            continue
        }

        if ($text -notmatch '[\s"]') {
            $quoted += $text
            continue
        }

        $escaped = $text -replace '\\(?=("+)$)', '\\' -replace '"', '\"'
        $quoted += '"' + $escaped + '"'
    }
    return ($quoted -join " ")
}

function Invoke-GateProcess {
    param(
        [string[]]$Command,
        [string]$WorkingDirectory,
        [string]$LogPath,
        [int]$TimeoutSeconds
    )

    $fileName = $Command[0]
    $arguments = @()
    if ($Command.Count -gt 1) {
        $arguments = @($Command[1..($Command.Count - 1)])
    }
    $argumentLine = ConvertTo-ProcessArgumentLine -Arguments $arguments

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo.FileName = $fileName
    $process.StartInfo.Arguments = $argumentLine
    $process.StartInfo.WorkingDirectory = $WorkingDirectory
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    $process.StartInfo.CreateNoWindow = $true

    $header = @(
        "command: $fileName $argumentLine",
        "working_directory: $WorkingDirectory",
        "timeout_seconds: $TimeoutSeconds",
        ""
    )

    try {
        [void]$process.Start()
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()

        $completed = $process.WaitForExit($TimeoutSeconds * 1000)
        if (-not $completed) {
            try {
                $process.Kill()
            } catch {
                # Best-effort cleanup after timeout.
            }
            $process.WaitForExit()
            $stdout = $stdoutTask.Result
            $stderr = $stderrTask.Result
            $body = @(
                "status: timeout",
                "exit_code: 124",
                "",
                "stdout:",
                $stdout,
                "",
                "stderr:",
                $stderr
            )
            Set-Content -Path $LogPath -Value ($header + $body) -Encoding UTF8
            return [pscustomobject]@{ ExitCode = 124; TimedOut = $true }
        }

        $stdout = $stdoutTask.Result
        $stderr = $stderrTask.Result
        $body = @(
            "status: completed",
            "exit_code: $($process.ExitCode)",
            "",
            "stdout:",
            $stdout,
            "",
            "stderr:",
            $stderr
        )
        Set-Content -Path $LogPath -Value ($header + $body) -Encoding UTF8
        return [pscustomobject]@{ ExitCode = $process.ExitCode; TimedOut = $false }
    } catch {
        $body = @(
            "status: failed-to-start",
            "exit_code: 127",
            "",
            "error:",
            $_.Exception.Message
        )
        Set-Content -Path $LogPath -Value ($header + $body) -Encoding UTF8
        return [pscustomobject]@{ ExitCode = 127; TimedOut = $false }
    } finally {
        if ($null -ne $process) {
            $process.Dispose()
        }
    }
}

function Get-ToolInstallHint {
    param([string]$ToolId)

    switch ($ToolId) {
        "python" { return "Install Python 3.11 and ensure python is on PATH." }
        "semgrep" { return "Run scripts/validation/install-python-deps.ps1, then ensure semgrep or pysemgrep is on PATH." }
        "import-linter" { return "Run scripts/validation/install-python-deps.ps1, then ensure lint-imports is on PATH." }
        "pydeps" { return "Run scripts/validation/install-python-deps.ps1, then ensure pydeps is on PATH." }
        "cppcheck" { return "Install cppcheck and ensure cppcheck is on PATH." }
        "git" { return "Install Git and ensure git is on PATH." }
        "powershell" { return "Install PowerShell and ensure powershell is on PATH." }
        "pyright" { return "Install pyright ahead of time, for example npm install -g pyright. Pre-push does not use npx fallback or download tools." }
        "pytest" { return "Run scripts/validation/install-python-deps.ps1, then ensure pytest is available through python -m pytest." }
        default { return "Install the required tool and ensure it is on PATH." }
    }
}

function Resolve-ValidationToolPath {
    param([string[]]$ToolNames)

    foreach ($toolName in @($ToolNames)) {
        $command = Get-Command -Name $toolName -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($null -ne $command) {
            if ([string]::IsNullOrWhiteSpace($command.Source)) {
                return $command.Name
            }
            return $command.Source
        }
    }

    $pythonCommand = Get-Command -Name python -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -eq $pythonCommand) {
        return $null
    }

    $scriptDirs = @()
    try {
        $userBase = (& $pythonCommand.Source -c "import site; print(site.USER_BASE)").Trim()
        $versionTag = (& $pythonCommand.Source -c "import sys; print(f'Python{sys.version_info.major}{sys.version_info.minor}')").Trim()
        if (-not [string]::IsNullOrWhiteSpace($userBase) -and -not [string]::IsNullOrWhiteSpace($versionTag)) {
            $scriptDirs += (Join-Path $userBase "$versionTag\Scripts")
        }
    } catch {
        # Fall through to the interpreter-local Scripts directory.
    }

    $pythonRoot = Split-Path -Path (Split-Path -Path $pythonCommand.Source -Parent) -Parent
    if (-not [string]::IsNullOrWhiteSpace($pythonRoot)) {
        $scriptDirs += (Join-Path $pythonRoot "Scripts")
    }

    foreach ($dir in @($scriptDirs | Select-Object -Unique)) {
        foreach ($toolName in @($ToolNames)) {
            foreach ($candidateName in @($toolName, "$toolName.exe")) {
                $candidatePath = Join-Path $dir $candidateName
                if (Test-Path -LiteralPath $candidatePath) {
                    return $candidatePath
                }
            }
        }
    }

    return $null
}

function Test-StepTools {
    param($Tools)

    $details = @()
    $missingRequired = @()
    $missingOptional = @()

    foreach ($tool in @($Tools)) {
        if ($null -eq $tool) {
            continue
        }

        $toolId = [string]$tool.id
        $names = ConvertTo-StringArray -Value $tool.names
        $required = $true
        if ($null -ne $tool.PSObject.Properties["required"] -and $tool.required -eq $false) {
            $required = $false
        }

        $resolved = Resolve-ValidationToolPath -ToolNames $names

        $record = [ordered]@{
            id = $toolId
            names = $names
            required = $required
            found = ($null -ne $resolved)
            resolved = $resolved
            hint = Get-ToolInstallHint -ToolId $toolId
        }
        $details += [pscustomobject]$record

        if ($null -eq $resolved) {
            if ($required) {
                $missingRequired += [pscustomobject]$record
            } else {
                $missingOptional += [pscustomobject]$record
            }
        }
    }

    return [pscustomobject]@{
        Details = @($details)
        MissingRequired = @($missingRequired)
        MissingOptional = @($missingOptional)
    }
}

function Test-RequiredArtifact {
    param([string]$Pattern)

    $resolvedPattern = Resolve-WorkspacePath -PathValue ($Pattern.Replace('/', [System.IO.Path]::DirectorySeparatorChar))
    if ($resolvedPattern -notmatch '[*?]') {
        return (Test-Path -LiteralPath $resolvedPattern)
    }

    $firstWildcard = $resolvedPattern.IndexOfAny([char[]]@("*", "?"))
    $staticPrefix = $resolvedPattern.Substring(0, $firstWildcard)
    $root = $staticPrefix
    while (-not [string]::IsNullOrWhiteSpace($root) -and -not (Test-Path -LiteralPath $root)) {
        $parent = Split-Path -Path $root -Parent
        if ($parent -eq $root) {
            break
        }
        $root = $parent
    }
    if ([string]::IsNullOrWhiteSpace($root) -or -not (Test-Path -LiteralPath $root)) {
        return $false
    }

    $normalizedPattern = $resolvedPattern.Replace('\', '/')
    $matches = Get-ChildItem -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName.Replace('\', '/') -like $normalizedPattern } |
        Select-Object -First 1
    return ($null -ne $matches)
}

function Test-RequiredArtifacts {
    param([string[]]$Patterns)

    $results = @()
    foreach ($pattern in @($Patterns)) {
        if ([string]::IsNullOrWhiteSpace($pattern)) {
            continue
        }
        $exists = Test-RequiredArtifact -Pattern $pattern
        $results += [pscustomobject]@{
            pattern = $pattern
            exists = $exists
        }
    }
    return @($results)
}

function Write-GateManifest {
    param(
        [string]$Path,
        [hashtable]$Manifest
    )

    $Manifest | ConvertTo-Json -Depth 12 | Set-Content -Path $Path -Encoding UTF8
}

function Write-GateSummary {
    param(
        [string]$SummaryJsonPath,
        [string]$SummaryMarkdownPath,
        [hashtable]$Summary
    )

    $Summary | ConvertTo-Json -Depth 12 | Set-Content -Path $SummaryJsonPath -Encoding UTF8

    $lines = @()
    $lines += "# Gate Summary"
    $lines += ""
    $lines += "- gate: ``$($Summary.gate)``"
    $lines += "- resolved_gate: ``$($Summary.resolved_gate)``"
    $lines += "- status: ``$($Summary.status)``"
    $lines += "- report_root: ``$($Summary.report_root)``"
    $lines += "- started_at: ``$($Summary.started_at)``"
    $lines += "- finished_at: ``$($Summary.finished_at)``"
    $lines += ""
    $lines += "| Step | Blocking | Status | Exit | DurationSeconds | Log |"
    $lines += "|---|---:|---|---:|---:|---|"
    foreach ($step in @($Summary.steps)) {
        $lines += "| ``$($step.id)`` | $($step.blocking) | ``$($step.status)`` | $($step.exit_code) | $($step.duration_seconds) | ``$($step.log_path)`` |"
    }
    if (@($Summary.failures).Count -gt 0) {
        $lines += ""
        $lines += "## Failures"
        foreach ($failure in @($Summary.failures)) {
            $lines += "- ``$($failure.step_id)``: $($failure.reason)"
        }
    }
    Set-Content -Path $SummaryMarkdownPath -Value $lines -Encoding UTF8
}

if (-not (Test-Path -LiteralPath $configPath)) {
    throw "Gate configuration not found: $configPath"
}
$ChangedScope = Expand-CommaSeparatedValues -Values $ChangedScope
$SelectedStep = Expand-CommaSeparatedValues -Values $SelectedStep
$SkipStep = Expand-CommaSeparatedValues -Values $SkipStep
$SkipLayer = Expand-CommaSeparatedValues -Values $SkipLayer
if ($ChangedScope.Count -eq 0 -and -not [string]::IsNullOrWhiteSpace($BaseSha) -and -not [string]::IsNullOrWhiteSpace($HeadSha)) {
    try {
        $ChangedScope = @(& git diff --name-only $BaseSha $HeadSha | ForEach-Object { ([string]$_).Trim() } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    } catch {
        throw "Unable to derive changed scope from BaseSha/HeadSha: $BaseSha..$HeadSha"
    }
}
if ($SkipStep.Count -gt 0 -and [string]::IsNullOrWhiteSpace($SkipJustification)) {
    throw "SkipJustification is required when SkipStep is not empty."
}
if ($SkipLayer.Count -gt 0 -and [string]::IsNullOrWhiteSpace($SkipJustification)) {
    throw "SkipJustification is required when SkipLayer is not empty."
}

$config = Get-Content -Raw -Path $configPath | ConvertFrom-Json
$requestedGateConfig = Get-JsonPropertyValue -Object $config.gates -Name $Gate
if ($null -eq $requestedGateConfig) {
    throw "Gate '$Gate' is not defined in $configPath"
}

$resolvedGate = $Gate
$gateConfig = $requestedGateConfig
if ($null -ne $requestedGateConfig.PSObject.Properties["aliasOf"] -and -not [string]::IsNullOrWhiteSpace($requestedGateConfig.aliasOf)) {
    $resolvedGate = [string]$requestedGateConfig.aliasOf
    $gateConfig = Get-JsonPropertyValue -Object $config.gates -Name $resolvedGate
    if ($null -eq $gateConfig) {
        throw "Gate '$Gate' aliases missing gate '$resolvedGate'."
    }
}

$defaultReportRoot = if ($null -ne $requestedGateConfig.PSObject.Properties["defaultReportRoot"]) {
    [string]$requestedGateConfig.defaultReportRoot
} else {
    [string]$gateConfig.defaultReportRoot
}
if ([string]::IsNullOrWhiteSpace($ReportRoot)) {
    $ReportRoot = $defaultReportRoot
}

$gateReportDir = Resolve-WorkspacePath -PathValue $ReportRoot
$logsDir = Join-Path $gateReportDir "logs"
New-Item -ItemType Directory -Force -Path $logsDir | Out-Null

if ($Gate -eq "pre-push") {
    if ($ChangedScope.Count -eq 0 -and [string]::IsNullOrWhiteSpace($BaseSha)) {
        try {
            $BaseSha = (& git rev-parse "HEAD~1").Trim()
            $HeadSha = "HEAD"
            $ChangedScope = @(& git diff --name-only $BaseSha $HeadSha | ForEach-Object { ([string]$_).Trim() } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
        } catch {
            throw "Unable to derive direct pre-push smoke range. Run invoke-pre-push-gate.ps1 for push-hook validation or pass -ChangedScope/-BaseSha/-HeadSha explicitly."
        }
    }
    if ([string]::IsNullOrWhiteSpace($ClassificationPath)) {
        $ClassificationPath = Join-Path $gateReportDir "pre-push-classification.json"
        $classifyChangeScript = Join-Path $PSScriptRoot "classify-change.ps1"
        $changedFileArgument = if ($ChangedScope.Count -gt 0) { $ChangedScope -join "," } else { "" }
        & $classifyChangeScript `
            -Mode "pre-push" `
            -OutputPath $ClassificationPath `
            -ChangedFile $changedFileArgument `
            -BaseSha $BaseSha `
            -HeadSha $HeadSha | Out-Null
        if (-not (Test-Path -LiteralPath $ClassificationPath)) {
            throw "Direct pre-push classification evidence generation failed: $ClassificationPath"
        }
    }
    if ($SelectedStep.Count -eq 0 -and (Test-Path -LiteralPath $ClassificationPath)) {
        $classification = Get-Content -Raw -Path $ClassificationPath | ConvertFrom-Json
        $selectedPrePushSteps = ConvertTo-StringArray -Value $classification.selected_pre_push_steps
        if ($selectedPrePushSteps.Count -gt 0) {
            $SelectedStep = @($selectedPrePushSteps)
        }
    }
}

$effectiveLane = if ($PSBoundParameters.ContainsKey("Lane")) { $Lane } else { Get-DefaultGateValue -Name "Lane" -GateName $Gate }
$effectiveRiskProfile = if ($PSBoundParameters.ContainsKey("RiskProfile")) { $RiskProfile } else { Get-DefaultGateValue -Name "RiskProfile" -GateName $Gate }
$effectiveDesiredDepth = if ($PSBoundParameters.ContainsKey("DesiredDepth")) { $DesiredDepth } else { Get-DefaultGateValue -Name "DesiredDepth" -GateName $Gate }

$runtime = @{
    Suite = @($Suite)
    Lane = $effectiveLane
    RiskProfile = $effectiveRiskProfile
    DesiredDepth = $effectiveDesiredDepth
    ChangedScope = @($ChangedScope)
    ClassificationPath = $ClassificationPath
    BaseSha = $BaseSha
    HeadSha = $HeadSha
    DirtyWorktree = $DirtyWorktree.IsPresent
    DirtyPolicy = $DirtyPolicy
    SkipLayer = @($SkipLayer)
    SkipJustification = $SkipJustification
    IncludeHardwareSmoke = $IncludeHardwareSmoke.IsPresent
    IncludeHilClosedLoop = $IncludeHilClosedLoop.IsPresent
    IncludeHilCaseMatrix = $IncludeHilCaseMatrix.IsPresent
    EnablePythonCoverage = $EnablePythonCoverage.IsPresent
    EnableCppCoverage = $EnableCppCoverage.IsPresent
}

$startedAt = (Get-Date).ToString("o")
$steps = @($gateConfig.steps)
if ($SelectedStep.Count -gt 0) {
    $selectedStepSet = @{}
    foreach ($selected in @($SelectedStep)) {
        if (-not [string]::IsNullOrWhiteSpace($selected)) {
            $selectedStepSet[$selected] = $true
        }
    }
    $missingSelectedSteps = @($SelectedStep | Where-Object {
        $requested = $_
        -not @($steps | Where-Object { [string]$_.id -eq $requested }).Count
    })
    if ($missingSelectedSteps.Count -gt 0) {
        throw "Selected pre-push step(s) are not defined in gate '$resolvedGate': $($missingSelectedSteps -join ', ')"
    }
    $steps = @($steps | Where-Object { $selectedStepSet.ContainsKey([string]$_.id) })
}
$manifestSteps = @()
foreach ($step in $steps) {
    $stepId = [string]$step.id
    $stepReportDir = Join-Path $gateReportDir $stepId
    $context = @{
        workspaceRoot = $workspaceRoot
        gateReportDir = $gateReportDir
        stepReportDir = $stepReportDir
        classificationPath = $ClassificationPath
        baseSha = $BaseSha
        headSha = $HeadSha
    }
    $manifestSteps += [pscustomobject]@{
        id = $stepId
        name = [string]$step.name
        blocking = [bool]$step.blocking
        timeout_seconds = [int]$step.timeoutSeconds
        log_path = (Join-Path $logsDir "$stepId.log")
        report_dir = $stepReportDir
        command = Expand-CommandTemplate -Command @($step.command) -Context $context -Runtime $runtime
        required_artifacts = @(ConvertTo-StringArray -Value $step.requiredArtifacts | ForEach-Object { Expand-TemplateValue -Value $_ -Context $context })
        requires_tool = @($step.requiresTool)
    }
}

$manifest = [ordered]@{
    schemaVersion = 1
    generated_at = $startedAt
    workspace_root = $workspaceRoot
    gate = $Gate
    resolved_gate = $resolvedGate
    config_path = $configPath
    report_root = $gateReportDir
    changed_scope = @($ChangedScope)
    selected_step = @($SelectedStep)
    classification_path = $ClassificationPath
    base_sha = $BaseSha
    head_sha = $HeadSha
    dirty_worktree = $DirtyWorktree.IsPresent
    dirty_policy = $DirtyPolicy
    skip_step = @($SkipStep)
    skip_justification = $SkipJustification
    force_native = $ForceNative.IsPresent
    force_hil = $ForceHil.IsPresent
    runtime = $runtime
    steps = @($manifestSteps)
}
Write-GateManifest -Path (Join-Path $gateReportDir "gate-manifest.json") -Manifest $manifest

$stepResults = @()
$failures = @()
$gateFailed = $false
$skipSet = @{}
foreach ($skip in @($SkipStep)) {
    if (-not [string]::IsNullOrWhiteSpace($skip)) {
        $skipSet[$skip] = $true
    }
}

foreach ($step in $manifestSteps) {
    $stepStart = Get-Date
    $logPath = [string]$step.log_path
    $stepReportDir = [string]$step.report_dir
    New-Item -ItemType Directory -Force -Path $stepReportDir | Out-Null

    if ($skipSet.ContainsKey($step.id)) {
        Set-Content -Path $logPath -Value @(
            "status: skipped",
            "step: $($step.id)",
            "justification: $SkipJustification"
        ) -Encoding UTF8
        $stepResults += [pscustomobject]@{
            id = $step.id
            name = $step.name
            blocking = $step.blocking
            status = "skipped"
            exit_code = 0
            timed_out = $false
            duration_seconds = 0
            log_path = $logPath
            report_dir = $stepReportDir
            required_artifacts = @()
            missing_artifacts = @()
            tools = @()
        }
        continue
    }

    $toolResult = Test-StepTools -Tools $step.requires_tool
    if (@($toolResult.MissingRequired).Count -gt 0) {
        $toolLines = @(
            "status: missing-required-tool",
            "step: $($step.id)",
            ""
        )
        foreach ($missing in @($toolResult.MissingRequired)) {
            $toolLines += "missing: $($missing.id) names=$($missing.names -join ', ')"
            $toolLines += "hint: $($missing.hint)"
        }
        foreach ($missing in @($toolResult.MissingOptional)) {
            $toolLines += "optional_missing: $($missing.id) names=$($missing.names -join ', ')"
            $toolLines += "hint: $($missing.hint)"
        }
        Set-Content -Path $logPath -Value $toolLines -Encoding UTF8
        $duration = [math]::Round(((Get-Date) - $stepStart).TotalSeconds, 3)
        $stepResults += [pscustomobject]@{
            id = $step.id
            name = $step.name
            blocking = $step.blocking
            status = "failed"
            exit_code = 127
            timed_out = $false
            duration_seconds = $duration
            log_path = $logPath
            report_dir = $stepReportDir
            required_artifacts = @()
            missing_artifacts = @()
            tools = @($toolResult.Details)
        }
        if ($step.blocking) {
            $gateFailed = $true
            $failures += [pscustomobject]@{
                step_id = $step.id
                reason = "required tool missing"
            }
            break
        }
        continue
    }

    $commandResult = Invoke-GateProcess -Command @($step.command) -WorkingDirectory $workspaceRoot -LogPath $logPath -TimeoutSeconds ([int]$step.timeout_seconds)
    $artifactResults = @()
    $missingArtifacts = @()
    $status = if ($commandResult.ExitCode -eq 0) { "passed" } else { "failed" }

    if ($commandResult.ExitCode -eq 0) {
        $artifactResults = Test-RequiredArtifacts -Patterns @($step.required_artifacts)
        $missingArtifacts = @($artifactResults | Where-Object { -not $_.exists } | ForEach-Object { $_.pattern })
        if (@($missingArtifacts).Count -gt 0) {
            $status = "failed"
        }
    }

    $duration = [math]::Round(((Get-Date) - $stepStart).TotalSeconds, 3)
    $stepResults += [pscustomobject]@{
        id = $step.id
        name = $step.name
        blocking = $step.blocking
        status = $status
        exit_code = $commandResult.ExitCode
        timed_out = $commandResult.TimedOut
        duration_seconds = $duration
        log_path = $logPath
        report_dir = $stepReportDir
        required_artifacts = @($artifactResults)
        missing_artifacts = @($missingArtifacts)
        tools = @($toolResult.Details)
    }

    if ($status -ne "passed") {
        $reason = if (@($missingArtifacts).Count -gt 0) {
            "required artifacts missing: $($missingArtifacts -join ', ')"
        } elseif ($commandResult.TimedOut) {
            "step timed out after $($step.timeout_seconds) seconds"
        } else {
            "command exited with $($commandResult.ExitCode)"
        }
        if ($step.blocking) {
            $gateFailed = $true
            $failures += [pscustomobject]@{
                step_id = $step.id
                reason = $reason
            }
            break
        } else {
            $failures += [pscustomobject]@{
                step_id = $step.id
                reason = "non-blocking: $reason"
            }
        }
    }
}

$gateArtifactResults = Test-RequiredArtifacts -Patterns @(ConvertTo-StringArray -Value $gateConfig.requiredArtifacts | ForEach-Object {
    Expand-TemplateValue -Value $_ -Context @{
        workspaceRoot = $workspaceRoot
        gateReportDir = $gateReportDir
        stepReportDir = $gateReportDir
    }
})
$missingGateArtifacts = @($gateArtifactResults | Where-Object { -not $_.exists } | ForEach-Object { $_.pattern })
if (@($missingGateArtifacts).Count -gt 0) {
    $gateFailed = $true
    $failures += [pscustomobject]@{
        step_id = "__gate__"
        reason = "required gate artifacts missing: $($missingGateArtifacts -join ', ')"
    }
}

$finishedAt = (Get-Date).ToString("o")
$summary = [ordered]@{
    schemaVersion = 1
    gate = $Gate
    resolved_gate = $resolvedGate
    status = if ($gateFailed) { "failed" } else { "passed" }
    report_root = $gateReportDir
    started_at = $startedAt
    finished_at = $finishedAt
    changed_scope = @($ChangedScope)
    selected_step = @($SelectedStep)
    classification_path = $ClassificationPath
    base_sha = $BaseSha
    head_sha = $HeadSha
    dirty_worktree = $DirtyWorktree.IsPresent
    dirty_policy = $DirtyPolicy
    skipped_steps = @($SkipStep)
    skip_justification = $SkipJustification
    force_native = $ForceNative.IsPresent
    force_hil = $ForceHil.IsPresent
    steps = @($stepResults)
    required_artifacts = @($gateArtifactResults)
    missing_artifacts = @($missingGateArtifacts)
    failures = @($failures)
}
Write-GateSummary `
    -SummaryJsonPath (Join-Path $gateReportDir "gate-summary.json") `
    -SummaryMarkdownPath (Join-Path $gateReportDir "gate-summary.md") `
    -Summary $summary

Write-Output "gate=$Gate status=$($summary.status) report=$gateReportDir"
if ($gateFailed) {
    exit 1
}
