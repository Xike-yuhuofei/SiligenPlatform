Set-StrictMode -Version Latest

$Script:AuditExitCodes = @{
    success                       = 0
    template_render_failed        = 10
    unresolved_template_variable  = 11
    invalid_prompt_selection      = 12
    codex_exec_failed             = 20
    output_file_missing           = 21
    output_json_invalid           = 22
    schema_validation_failed      = 23
    missing_report_files          = 30
    audit_index_not_planner_ready = 31
    run_summary_generation_failed = 40
}

$Script:AuditDefinitions = @(
    [PSCustomObject]@{
        PromptFile    = "audit-scope.md"
        PromptName    = "audit-scope"
        SkillName     = "audit-module-responsibility"
        Dimension     = "module_responsibility"
        Label         = "Module responsibility and semantic closure"
    },
    [PSCustomObject]@{
        PromptFile    = "audit-boundary.md"
        PromptName    = "audit-boundary"
        SkillName     = "audit-module-boundary"
        Dimension     = "module_boundary"
        Label         = "Module boundary and information hiding"
    },
    [PSCustomObject]@{
        PromptFile    = "audit-dependency.md"
        PromptName    = "audit-dependency"
        SkillName     = "audit-dependency-direction"
        Dimension     = "dependency_direction"
        Label         = "Dependency direction and stability"
    },
    [PSCustomObject]@{
        PromptFile    = "audit-contract.md"
        PromptName    = "audit-contract"
        SkillName     = "audit-interface-contract"
        Dimension     = "interface_contract"
        Label         = "Interface contract and interaction shape"
    },
    [PSCustomObject]@{
        PromptFile    = "audit-ownership.md"
        PromptName    = "audit-ownership"
        SkillName     = "audit-domain-model-ownership"
        Dimension     = "domain_model_ownership"
        Label         = "Domain model and data ownership"
    },
    [PSCustomObject]@{
        PromptFile    = "audit-orchestration.md"
        PromptName    = "audit-orchestration"
        SkillName     = "audit-control-orchestration"
        Dimension     = "control_orchestration"
        Label         = "Control and orchestration placement"
    },
    [PSCustomObject]@{
        PromptFile    = "audit-isolation.md"
        PromptName    = "audit-isolation"
        SkillName     = "audit-change-isolation"
        Dimension     = "change_isolation"
        Label         = "Change isolation and evolution boundaries"
    },
    [PSCustomObject]@{
        PromptFile    = "audit-legacy.md"
        PromptName    = "audit-legacy"
        SkillName     = "audit-legacy-compat"
        Dimension     = "legacy_compat"
        Label         = "Legacy and compatibility governance"
    },
    [PSCustomObject]@{
        PromptFile    = "audit-fallback.md"
        PromptName    = "audit-fallback"
        SkillName     = "audit-fallback-degradation"
        Dimension     = "fallback_degradation"
        Label         = "Fallback and degradation paths"
    },
    [PSCustomObject]@{
        PromptFile    = "audit-observability.md"
        PromptName    = "audit-observability"
        SkillName     = "audit-observability-diagnostics"
        Dimension     = "observability_diagnostics"
        Label         = "Observability and diagnostics"
    },
    [PSCustomObject]@{
        PromptFile    = "audit-placement.md"
        PromptName    = "audit-placement"
        SkillName     = "audit-build-deployment-alignment"
        Dimension     = "build_deployment_alignment"
        Label         = "Build, deployment, and implementation placement"
    },
    [PSCustomObject]@{
        PromptFile    = "audit-verification.md"
        PromptName    = "audit-verification"
        SkillName     = "audit-test-boundary-validation"
        Dimension     = "test_boundary_validation"
        Label         = "Test boundary and validation ownership"
    }
)

function Get-AuditDefinitions {
    return $Script:AuditDefinitions
}

function Resolve-AuditPromptSelection {
    param(
        [AllowNull()]
        [string[]]$PromptNames
    )

    $definitions = @(Get-AuditDefinitions)
    if ($null -eq $PromptNames -or $PromptNames.Count -eq 0) {
        return [PSCustomObject]@{
            SelectionSpecified    = $false
            RequestedPromptNames = @()
            Audits               = $definitions
            InvalidPromptNames   = @()
        }
    }

    $normalizedPromptNames = [System.Collections.Generic.List[string]]::new()
    foreach ($promptName in $PromptNames) {
        if ($null -eq $promptName) {
            continue
        }

        foreach ($segment in ([string]$promptName -split ',')) {
            $trimmed = $segment.Trim()
            if (-not [string]::IsNullOrWhiteSpace($trimmed)) {
                $normalizedPromptNames.Add($trimmed)
            }
        }
    }

    $definitionMap = @{}
    foreach ($definition in $definitions) {
        $definitionMap[[string]$definition.PromptName] = $definition
    }

    $invalidPromptNames = [System.Collections.Generic.List[string]]::new()
    foreach ($promptName in $normalizedPromptNames) {
        if (-not $definitionMap.ContainsKey($promptName)) {
            $invalidPromptNames.Add($promptName)
        }
    }

    $selectedAudits = [System.Collections.Generic.List[object]]::new()
    foreach ($definition in $definitions) {
        if ($normalizedPromptNames.Contains([string]$definition.PromptName)) {
            $selectedAudits.Add($definition)
        }
    }

    return [PSCustomObject]@{
        SelectionSpecified    = $true
        RequestedPromptNames = @($normalizedPromptNames)
        Audits               = @($selectedAudits)
        InvalidPromptNames   = @($invalidPromptNames | Select-Object -Unique)
    }
}

function Get-AuditExitCode {
    param(
        [Parameter(Mandatory)]
        [string]$Name
    )

    if (-not $Script:AuditExitCodes.ContainsKey($Name)) {
        throw "Unknown audit exit code name: $Name"
    }

    return [int]$Script:AuditExitCodes[$Name]
}

function Ensure-ParentDirectory {
    param(
        [Parameter(Mandatory)]
        [string]$Path
    )

    $parent = Split-Path -Path $Path -Parent
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
}

function ConvertTo-AbsolutePath {
    param(
        [Parameter(Mandatory)]
        [string]$Path
    )

    return [System.IO.Path]::GetFullPath($Path)
}

function Test-CodexProfileExists {
    param(
        [string]$Name
    )

    if ([string]::IsNullOrWhiteSpace($Name)) {
        return $false
    }

    $configPath = Join-Path $HOME ".codex\config.toml"
    if (-not (Test-Path -LiteralPath $configPath)) {
        return $false
    }

    $pattern = '^\s*\[profiles\.' + [regex]::Escape($Name) + '\]\s*$'
    return [bool](Select-String -Path $configPath -Pattern $pattern)
}

function Read-JsonFile {
    param(
        [Parameter(Mandatory)]
        [string]$Path
    )

    $raw = Get-Content -LiteralPath $Path -Raw -Encoding utf8
    return $raw | ConvertFrom-Json -AsHashtable
}

function Write-JsonFile {
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [object]$Data
    )

    Ensure-ParentDirectory -Path $Path
    $json = $Data | ConvertTo-Json -Depth 64
    Set-Content -LiteralPath $Path -Value $json -Encoding utf8
}

function Render-PromptTemplate {
    param(
        [Parameter(Mandatory)]
        [string]$TemplatePath,

        [Parameter(Mandatory)]
        [hashtable]$Variables,

        [Parameter(Mandatory)]
        [string]$RenderedPath
    )

    try {
        $template = Get-Content -LiteralPath $TemplatePath -Raw -Encoding utf8
    } catch {
        throw "Failed to read prompt template '$TemplatePath': $($_.Exception.Message)"
    }

    foreach ($key in $Variables.Keys) {
        $placeholder = "{{{{{0}}}}}" -f $key
        $value = [string]$Variables[$key]
        $escapedPlaceholder = [regex]::Escape($placeholder)
        $template = [regex]::Replace($template, $escapedPlaceholder, [System.Text.RegularExpressions.MatchEvaluator]{ param($m) $value })
    }

    $unresolved = @([regex]::Matches($template, "\{\{[A-Z0-9_]+\}\}") | ForEach-Object { $_.Value } | Select-Object -Unique)
    if ($unresolved.Count -gt 0) {
        throw "Unresolved template variables in '$TemplatePath': $($unresolved -join ', ')"
    }

    Ensure-ParentDirectory -Path $RenderedPath
    Set-Content -LiteralPath $RenderedPath -Value $template -Encoding utf8
}

function Get-RenderedPromptPath {
    param(
        [Parameter(Mandatory)]
        [string]$ArtifactsRoot,

        [Parameter(Mandatory)]
        [string]$RunId,

        [Parameter(Mandatory)]
        [string]$ModuleName,

        [Parameter(Mandatory)]
        [string]$PromptName
    )

    return (Join-Path $ArtifactsRoot (Join-Path $RunId (Join-Path "_rendered" ("{0}-{1}.md" -f $ModuleName, $PromptName))))
}

function Get-ReportPath {
    param(
        [Parameter(Mandatory)]
        [string]$ArtifactsRoot,

        [Parameter(Mandatory)]
        [string]$RunId,

        [Parameter(Mandatory)]
        [string]$ModuleName,

        [Parameter(Mandatory)]
        [string]$PromptName
    )

    return (Join-Path $ArtifactsRoot (Join-Path $RunId (Join-Path $ModuleName (Join-Path $PromptName "report.json"))))
}

function Get-ModuleAuditIndexPath {
    param(
        [Parameter(Mandatory)]
        [string]$ArtifactsRoot,

        [Parameter(Mandatory)]
        [string]$RunId,

        [Parameter(Mandatory)]
        [string]$ModuleName
    )

    return (Join-Path $ArtifactsRoot (Join-Path $RunId (Join-Path $ModuleName "module-audit-index.json")))
}

function Get-ModuleOptimizationPlanPath {
    param(
        [Parameter(Mandatory)]
        [string]$ArtifactsRoot,

        [Parameter(Mandatory)]
        [string]$RunId,

        [Parameter(Mandatory)]
        [string]$ModuleName
    )

    return (Join-Path $ArtifactsRoot (Join-Path $RunId (Join-Path $ModuleName (Join-Path "audit-remediation" "module-optimization-plan.json"))))
}

function Get-RunSummaryPath {
    param(
        [Parameter(Mandatory)]
        [string]$ArtifactsRoot,

        [Parameter(Mandatory)]
        [string]$RunId
    )

    return (Join-Path $ArtifactsRoot (Join-Path $RunId "run-summary.json"))
}

function Test-JsonValueAgainstSchemaInternal {
    param(
        [Parameter(Mandatory)]
        [AllowNull()]
        [object]$Value,

        [Parameter(Mandatory)]
        [hashtable]$Schema,

        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [System.Collections.Generic.List[string]]$Errors
    )

    $schemaType = $null
    if ($Schema.ContainsKey("type")) {
        $schemaType = [string]$Schema["type"]
    }

    if ($Schema.ContainsKey("const")) {
        if ($Value -ne $Schema["const"]) {
            $Errors.Add(("{0}: expected const '{1}'" -f $Path, $Schema["const"]))
            return
        }
    }

    if ($Schema.ContainsKey("enum")) {
        $allowed = @($Schema["enum"])
        if ($allowed -notcontains $Value) {
            $Errors.Add(("{0}: value '{1}' is not in enum" -f $Path, $Value))
        }
    }

    switch ($schemaType) {
        "object" {
            if ($Value -isnot [System.Collections.IDictionary]) {
                $Errors.Add(("{0}: expected object" -f $Path))
                return
            }

            $valueMap = [System.Collections.IDictionary]$Value
            $properties = @{}
            if ($Schema.ContainsKey("properties") -and $Schema["properties"] -is [System.Collections.IDictionary]) {
                $properties = [hashtable]$Schema["properties"]
            }

            if ($Schema.ContainsKey("required")) {
                foreach ($requiredKey in @($Schema["required"])) {
                    if (-not $valueMap.Contains($requiredKey)) {
                        $Errors.Add(("{0}: missing required property '{1}'" -f $Path, $requiredKey))
                    }
                }
            }

            $additionalProperties = $true
            if ($Schema.ContainsKey("additionalProperties")) {
                $additionalProperties = [bool]$Schema["additionalProperties"]
            }

            foreach ($key in $valueMap.Keys) {
                if ($properties.ContainsKey($key)) {
                    Test-JsonValueAgainstSchemaInternal -Value $valueMap[$key] -Schema ([hashtable]$properties[$key]) -Path ("{0}.{1}" -f $Path, $key) -Errors $Errors
                    continue
                }

                if (-not $additionalProperties) {
                    $Errors.Add(("{0}: unexpected property '{1}'" -f $Path, $key))
                }
            }
        }
        "array" {
            if ($Value -isnot [System.Collections.IList]) {
                $Errors.Add(("{0}: expected array" -f $Path))
                return
            }

            $items = [System.Collections.IList]$Value
            if ($Schema.ContainsKey("minItems") -and $items.Count -lt [int]$Schema["minItems"]) {
                $Errors.Add(("{0}: expected at least {1} items" -f $Path, $Schema["minItems"]))
            }
            if ($Schema.ContainsKey("maxItems") -and $items.Count -gt [int]$Schema["maxItems"]) {
                $Errors.Add(("{0}: expected at most {1} items" -f $Path, $Schema["maxItems"]))
            }

            if ($Schema.ContainsKey("items") -and $Schema["items"] -is [System.Collections.IDictionary]) {
                $itemSchema = [hashtable]$Schema["items"]
                for ($i = 0; $i -lt $items.Count; $i++) {
                    Test-JsonValueAgainstSchemaInternal -Value $items[$i] -Schema $itemSchema -Path ("{0}[{1}]" -f $Path, $i) -Errors $Errors
                }
            }
        }
        "string" {
            if ($Value -isnot [string]) {
                $Errors.Add(("{0}: expected string" -f $Path))
                return
            }

            $text = [string]$Value
            if ($Schema.ContainsKey("minLength") -and $text.Length -lt [int]$Schema["minLength"]) {
                $Errors.Add(("{0}: string shorter than minLength {1}" -f $Path, $Schema["minLength"]))
            }
            if ($Schema.ContainsKey("maxLength") -and $text.Length -gt [int]$Schema["maxLength"]) {
                $Errors.Add(("{0}: string longer than maxLength {1}" -f $Path, $Schema["maxLength"]))
            }
            if ($Schema.ContainsKey("pattern") -and -not [regex]::IsMatch($text, [string]$Schema["pattern"])) {
                $Errors.Add(("{0}: string does not match pattern {1}" -f $Path, $Schema["pattern"]))
            }
        }
        "integer" {
            $isInteger = $Value -is [sbyte] -or
                $Value -is [byte] -or
                $Value -is [int16] -or
                $Value -is [uint16] -or
                $Value -is [int32] -or
                $Value -is [uint32] -or
                $Value -is [int64] -or
                $Value -is [uint64]

            if (-not $isInteger) {
                $Errors.Add(("{0}: expected integer" -f $Path))
                return
            }

            $number = [long]$Value
            if ($Schema.ContainsKey("minimum") -and $number -lt [long]$Schema["minimum"]) {
                $Errors.Add(("{0}: integer less than minimum {1}" -f $Path, $Schema["minimum"]))
            }
            if ($Schema.ContainsKey("maximum") -and $number -gt [long]$Schema["maximum"]) {
                $Errors.Add(("{0}: integer greater than maximum {1}" -f $Path, $Schema["maximum"]))
            }
        }
        "boolean" {
            if ($Value -isnot [bool]) {
                $Errors.Add(("{0}: expected boolean" -f $Path))
            }
        }
        default {
            if ($Schema.ContainsKey("properties") -and $Value -is [System.Collections.IDictionary]) {
                Test-JsonValueAgainstSchemaInternal -Value $Value -Schema (@{ type = "object"; properties = $Schema["properties"]; required = $Schema["required"]; additionalProperties = $Schema["additionalProperties"] }) -Path $Path -Errors $Errors
            }
        }
    }
}

function Test-JsonAgainstSchema {
    param(
        [Parameter(Mandatory)]
        [string]$JsonPath,

        [Parameter(Mandatory)]
        [string]$SchemaPath
    )

    if (-not (Test-Path -LiteralPath $JsonPath)) {
        return [PSCustomObject]@{
            IsValid     = $false
            FailureCode = (Get-AuditExitCode -Name "output_file_missing")
            Errors      = @("JSON file missing: $JsonPath")
        }
    }

    if (-not (Test-Path -LiteralPath $SchemaPath)) {
        return [PSCustomObject]@{
            IsValid     = $false
            FailureCode = (Get-AuditExitCode -Name "schema_validation_failed")
            Errors      = @("Schema file missing: $SchemaPath")
        }
    }

    try {
        $value = Read-JsonFile -Path $JsonPath
    } catch {
        return [PSCustomObject]@{
            IsValid     = $false
            FailureCode = (Get-AuditExitCode -Name "output_json_invalid")
            Errors      = @("Invalid JSON in '$JsonPath': $($_.Exception.Message)")
        }
    }

    try {
        $schema = Read-JsonFile -Path $SchemaPath
    } catch {
        return [PSCustomObject]@{
            IsValid     = $false
            FailureCode = (Get-AuditExitCode -Name "schema_validation_failed")
            Errors      = @("Invalid schema JSON in '$SchemaPath': $($_.Exception.Message)")
        }
    }

    $errors = [System.Collections.Generic.List[string]]::new()
    Test-JsonValueAgainstSchemaInternal -Value $value -Schema $schema -Path '$' -Errors $errors

    if ($errors.Count -gt 0) {
        return [PSCustomObject]@{
            IsValid     = $false
            FailureCode = (Get-AuditExitCode -Name "schema_validation_failed")
            Errors      = @($errors)
        }
    }

    return [PSCustomObject]@{
        IsValid     = $true
        FailureCode = (Get-AuditExitCode -Name "success")
        Errors      = @()
    }
}

function Invoke-CodexExecForPrompt {
    param(
        [Parameter(Mandatory)]
        [string]$PromptPath,

        [Parameter(Mandatory)]
        [string]$RepoRoot,

        [Parameter(Mandatory)]
        [string]$SchemaPath,

        [Parameter(Mandatory)]
        [string]$OutputPath,

        [ValidateSet("model-and-local", "local-only")]
        [string]$SchemaEnforcementMode = "local-only",

        [string]$Profile,

        [string]$Model,

        [string]$Reasoning
    )

    Ensure-ParentDirectory -Path $OutputPath

    $codexArgs = @(
        "exec",
        "--cd", (ConvertTo-AbsolutePath -Path $RepoRoot),
        "--sandbox", "read-only",
        "-o", (ConvertTo-AbsolutePath -Path $OutputPath),
        "-"
    )

    if ($SchemaEnforcementMode -eq "model-and-local") {
        $codexArgs = @(
            "exec",
            "--cd", (ConvertTo-AbsolutePath -Path $RepoRoot),
            "--sandbox", "read-only",
            "--output-schema", (ConvertTo-AbsolutePath -Path $SchemaPath),
            "-o", (ConvertTo-AbsolutePath -Path $OutputPath),
            "-"
        )
    }

    if (Test-CodexProfileExists -Name $Profile) {
        $codexArgs = @("exec", "--profile", $Profile) + $codexArgs[1..($codexArgs.Count - 1)]
    }

    if (-not [string]::IsNullOrWhiteSpace($Model)) {
        $codexArgs = @("exec", "-m", $Model) + $codexArgs[1..($codexArgs.Count - 1)]
    }

    $prompt = Get-Content -LiteralPath $PromptPath -Raw -Encoding utf8
    if (-not [string]::IsNullOrWhiteSpace($Reasoning)) {
        $prompt = $prompt + [Environment]::NewLine + [Environment]::NewLine + "Reasoning effort hint: $Reasoning"
    }

    $null = $prompt | & codex @codexArgs
    return $LASTEXITCODE
}

function Get-HighestSeverity {
    param(
        [Parameter(Mandatory)]
        [string[]]$Severities
    )

    $severityOrder = @{
        none     = 0
        low      = 1
        medium   = 2
        high     = 3
        critical = 4
    }

    $winner = "none"
    foreach ($severity in $Severities) {
        if (-not $severityOrder.ContainsKey($severity)) {
            continue
        }
        if ($severityOrder[$severity] -gt $severityOrder[$winner]) {
            $winner = $severity
        }
    }
    return $winner
}
