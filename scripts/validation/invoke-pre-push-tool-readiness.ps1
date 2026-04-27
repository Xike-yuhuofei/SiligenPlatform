[CmdletBinding()]
param(
    [string]$ReportDir = "tests\reports\pre-push-gate\tool-readiness",
    [string]$ClassificationPath = "",
    [string[]]$ChangedFile = @()
)

$ErrorActionPreference = "Stop"
$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$gatesPath = Join-Path $PSScriptRoot "gates\gates.json"

function Resolve-WorkspacePath {
    param([string]$PathValue)
    if ([System.IO.Path]::IsPathRooted($PathValue)) { return [System.IO.Path]::GetFullPath($PathValue) }
    return [System.IO.Path]::GetFullPath((Join-Path $workspaceRoot $PathValue))
}

function Expand-CommaSeparatedValues {
    param([string[]]$Values)
    $expanded = @()
    foreach ($value in @($Values)) {
        if ([string]::IsNullOrWhiteSpace($value)) { continue }
        $expanded += @(([string]$value).Split(",") | ForEach-Object { $_.Trim() } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    }
    return @($expanded)
}

function ConvertTo-StringArray {
    param($Value)
    if ($null -eq $Value) { return @() }
    if ($Value -is [System.Array]) { return @($Value | ForEach-Object { [string]$_ }) }
    return @([string]$Value)
}

function Get-ToolHint {
    param([string]$ToolId)
    switch ($ToolId) {
        "git" { return "Install Git and ensure git is on PATH." }
        "gh" { return "Install GitHub CLI and ensure gh authentication is available for PR state checks." }
        "python" { return "Install Python 3.11 and ensure python is on PATH." }
        "powershell" { return "Install PowerShell and ensure powershell or pwsh is on PATH." }
        "pyright" { return "Install pyright ahead of time, for example npm install -g pyright. Pre-push does not use npx fallback." }
        default { return "Install the required tool and ensure it is on PATH." }
    }
}

function Test-PythonModule {
    param(
        [string]$ModuleName,
        [string]$Hint
    )
    $probe = "import importlib.util, sys; sys.exit(0 if importlib.util.find_spec('$ModuleName') is not None else 1)"
    python -c $probe | Out-Null
    return [pscustomobject]@{
        id = "python-module:$ModuleName"
        names = @($ModuleName)
        required = $true
        found = ($LASTEXITCODE -eq 0)
        resolved = ""
        hint = $Hint
    }
}

function Test-YamlParser {
    python -c "import importlib.util, sys; sys.exit(0 if (importlib.util.find_spec('yaml') is not None or importlib.util.find_spec('ruamel.yaml') is not None) else 1)" | Out-Null
    return [pscustomobject]@{
        id = "python-module:yaml-parser"
        names = @("PyYAML", "ruamel.yaml")
        required = $true
        found = ($LASTEXITCODE -eq 0)
        resolved = ""
        hint = "Install PyYAML or ruamel.yaml; pre-push does not download YAML tooling at runtime."
    }
}

$reportDirPath = Resolve-WorkspacePath -PathValue $ReportDir
New-Item -ItemType Directory -Force -Path $reportDirPath | Out-Null
$jsonPath = Join-Path $reportDirPath "pre-push-tool-readiness.json"
$mdPath = Join-Path $reportDirPath "pre-push-tool-readiness.md"

if ([string]::IsNullOrWhiteSpace($ClassificationPath)) {
    throw "ClassificationPath is required for selected-step tool readiness."
}
$classificationFile = Resolve-WorkspacePath -PathValue $ClassificationPath
if (-not (Test-Path -LiteralPath $classificationFile)) {
    throw "classification evidence missing: $classificationFile"
}
if (-not (Test-Path -LiteralPath $gatesPath)) {
    throw "gate configuration missing: $gatesPath"
}

$classification = Get-Content -Raw -Path $classificationFile | ConvertFrom-Json
$selectedSteps = @(ConvertTo-StringArray -Value $classification.selected_pre_push_steps)
$changedFiles = @(Expand-CommaSeparatedValues -Values $ChangedFile | ForEach-Object { ([string]$_).Replace('\', '/') } | Select-Object -Unique)
$gates = Get-Content -Raw -Path $gatesPath | ConvertFrom-Json
$prePushSteps = @($gates.gates."pre-push".steps)
$stepById = @{}
foreach ($step in $prePushSteps) {
    $stepById[[string]$step.id] = $step
}

$checks = @()
foreach ($stepId in $selectedSteps) {
    if (-not $stepById.ContainsKey($stepId)) {
        $checks += [pscustomobject]@{
            id = "step:$stepId"
            names = @($stepId)
            required = $true
            found = $false
            resolved = ""
            hint = "Selected step is not defined in gates.json."
        }
        continue
    }
    foreach ($tool in @($stepById[$stepId].requiresTool)) {
        if ($null -eq $tool) { continue }
        $required = $true
        if ($null -ne $tool.PSObject.Properties["required"] -and $tool.required -eq $false) {
            $required = $false
        }
        $names = ConvertTo-StringArray -Value $tool.names
        $resolved = $null
        foreach ($name in $names) {
            $command = Get-Command -Name $name -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($null -ne $command) {
                $resolved = if ([string]::IsNullOrWhiteSpace($command.Source)) { $command.Name } else { $command.Source }
                break
            }
        }
        $checks += [pscustomobject]@{
            id = [string]$tool.id
            names = @($names)
            required = $required
            found = ($null -ne $resolved)
            resolved = $resolved
            hint = Get-ToolHint -ToolId ([string]$tool.id)
        }
    }
}

$selectedSet = @{}
foreach ($stepId in $selectedSteps) { $selectedSet[$stepId] = $true }

if ($selectedSet.ContainsKey("hmi-runtime-gateway-protocol-compat") -or
    $selectedSet.ContainsKey("state-machine-interlock-contract") -or
    $selectedSet.ContainsKey("validation-system-contract") -or
    $selectedSet.ContainsKey("contracts-quick")) {
    $checks += Test-PythonModule -ModuleName "pytest" -Hint "Run scripts/validation/install-python-deps.ps1 so pytest is importable via python -m pytest."
}

if ($selectedSet.ContainsKey("recipe-config-schema-quick")) {
    $checks += Test-PythonModule -ModuleName "jsonschema" -Hint "Run scripts/validation/install-python-deps.ps1 so jsonschema is available for schema compatibility checks."
}

$changedYaml = @($changedFiles | Where-Object { $_ -match '\.ya?ml$' })
if ($changedYaml.Count -gt 0 -or $selectedSet.ContainsKey("recipe-config-schema-quick")) {
    $checks += Test-YamlParser
}

$deduped = @()
$seen = @{}
foreach ($check in $checks) {
    $key = "$($check.id)|$($check.names -join ',')"
    if (-not $seen.ContainsKey($key)) {
        $seen[$key] = $true
        $deduped += $check
    }
}
$missing = @($deduped | Where-Object { $_.required -and -not $_.found })
$status = if ($missing.Count -eq 0) { "passed" } else { "failed" }

$summary = [ordered]@{
    schemaVersion = 1
    generated_at = (Get-Date).ToString("o")
    status = $status
    classification_path = $classificationFile
    selected_pre_push_steps = @($selectedSteps)
    changed_files = @($changedFiles)
    checks = @($deduped)
    missing_required = @($missing)
}
$summary | ConvertTo-Json -Depth 12 | Set-Content -Path $jsonPath -Encoding UTF8

$lines = @(
    "# Pre-push Tool Readiness",
    "",
    "- status: ``$status``",
    "- classification_path: ``$classificationFile``",
    "- selected_pre_push_steps: ``$($selectedSteps -join ',')``",
    "",
    "| Required | Found | Tool | Hint |",
    "|---:|---:|---|---|"
)
foreach ($check in $deduped) {
    $lines += "| $($check.required) | $($check.found) | ``$($check.id)`` | $($check.hint) |"
}
$lines | Set-Content -Path $mdPath -Encoding UTF8

Write-Output "pre-push tool-readiness=$status report=$reportDirPath"
if ($status -ne "passed") { exit 1 }
