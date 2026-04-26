[CmdletBinding()]
param(
    [string]$ReportDir = "tests\reports\dependency-graphs",
    [switch]$SoftFail
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "tooling-common.ps1")

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$resolvedReportDir = Ensure-WorkspaceDirectory -Path (Resolve-WorkspaceReportPath -WorkspaceRoot $workspaceRoot -ReportPath $ReportDir)
$cmakeReportDir = Ensure-WorkspaceDirectory -Path (Join-Path $resolvedReportDir "cmake")
$pythonReportDir = Ensure-WorkspaceDirectory -Path (Join-Path $resolvedReportDir "python")
$cmakeBuildDir = Ensure-WorkspaceDirectory -Path (Join-Path $workspaceRoot "build\dependency-graphs-cmake")
$summaryPath = Join-Path $resolvedReportDir "dependency-graphs-summary.md"

function Get-QuotedGraphTokens {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Line
    )

    return [regex]::Matches($Line, '"([^"]+)"') | ForEach-Object { $_.Groups[1].Value }
}

function Write-TargetSubsetGraph {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourceDotPath,
        [Parameter(Mandatory = $true)]
        [string]$TargetName,
        [Parameter(Mandatory = $true)]
        [string]$OutputPath
    )

    $lines = Get-Content -LiteralPath $SourceDotPath
    $edgeMap = @{}
    $reverseEdgeMap = @{}
    $nodeDefinitionMap = @{}
    $labelToNodeMap = @{}

    foreach ($line in $lines) {
        $trimmed = $line.Trim()
        $tokens = @(Get-QuotedGraphTokens -Line $trimmed)
        if ($trimmed -match '->' -and $tokens.Count -ge 2) {
            $from = $tokens[0]
            $to = $tokens[1]
            if (-not $edgeMap.ContainsKey($from)) {
                $edgeMap[$from] = [System.Collections.Generic.HashSet[string]]::new()
            }
            if (-not $reverseEdgeMap.ContainsKey($to)) {
                $reverseEdgeMap[$to] = [System.Collections.Generic.HashSet[string]]::new()
            }
            $null = $edgeMap[$from].Add($to)
            $null = $reverseEdgeMap[$to].Add($from)
        }
        elseif ($tokens.Count -ge 1 -and $trimmed -match '\[') {
            $nodeId = $tokens[0]
            $nodeDefinitionMap[$nodeId] = $line
            $labelMatch = [regex]::Match($line, 'label\s*=\s*"([^"]+)"')
            if ($labelMatch.Success) {
                $primaryLabel = ($labelMatch.Groups[1].Value -split '\\n', 2)[0]
                if (-not [string]::IsNullOrWhiteSpace($primaryLabel) -and -not $labelToNodeMap.ContainsKey($primaryLabel)) {
                    $labelToNodeMap[$primaryLabel] = $nodeId
                }
            }
        }
    }

    $targetNodeId = if ($nodeDefinitionMap.ContainsKey($TargetName)) {
        $TargetName
    }
    elseif ($labelToNodeMap.ContainsKey($TargetName)) {
        [string]$labelToNodeMap[$TargetName]
    }
    else {
        $null
    }

    if ([string]::IsNullOrWhiteSpace($targetNodeId) -and -not $edgeMap.ContainsKey($TargetName) -and -not $reverseEdgeMap.ContainsKey($TargetName)) {
        return $false
    }

    $queue = [System.Collections.Generic.Queue[string]]::new()
    $visited = [System.Collections.Generic.HashSet[string]]::new()
    $seedNode = if ([string]::IsNullOrWhiteSpace($targetNodeId)) { $TargetName } else { $targetNodeId }
    $queue.Enqueue($seedNode)
    $null = $visited.Add($seedNode)

    while ($queue.Count -gt 0) {
        $current = $queue.Dequeue()
        foreach ($map in @($edgeMap, $reverseEdgeMap)) {
            if (-not $map.ContainsKey($current)) {
                continue
            }
            foreach ($next in $map[$current]) {
                if ($visited.Add($next)) {
                    $queue.Enqueue($next)
                }
            }
        }
    }

    $outputLines = @(
        "digraph `"$TargetName`" {"
    )
    foreach ($node in ($visited | Sort-Object)) {
        if ($nodeDefinitionMap.ContainsKey($node)) {
            $outputLines += $nodeDefinitionMap[$node]
        }
        else {
            $outputLines += ('    "{0}";' -f $node)
        }
    }
    foreach ($from in ($edgeMap.Keys | Sort-Object)) {
        foreach ($to in ($edgeMap[$from] | Sort-Object)) {
            if ($visited.Contains($from) -and $visited.Contains($to)) {
                $outputLines += ('    "{0}" -> "{1}";' -f $from, $to)
            }
        }
    }
    $outputLines += "}"
    Set-Content -LiteralPath $OutputPath -Value ($outputLines -join "`r`n") -Encoding UTF8
    return $true
}

$summaryLines = @(
    '# Dependency Graph Export',
    '',
    ('- failure_policy: `{0}`' -f $(if ($SoftFail) { 'soft-fail on graph tooling gaps' } else { 'blocking on graph tooling gaps and export failures' })),
    '- cmake_report_dir: `tests/reports/dependency-graphs/cmake`',
    '- python_report_dir: `tests/reports/dependency-graphs/python`',
    ''
)

$cmakeStatus = "skipped"
$cmakeDetail = ""
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if ($null -eq $cmakeCommand) {
    if ($SoftFail) {
        $cmakeStatus = "tool-missing"
        $cmakeDetail = "cmake command not found"
    }
    else {
        throw "CMake command not found; dependency graph export is a blocking gate."
    }
}
else {
    $totalDotPath = Join-Path $cmakeReportDir "workspace-targets.dot"
    $configureArgs = @(
        "-S",
        $workspaceRoot,
        "-B",
        $cmakeBuildDir,
        "-DSILIGEN_BUILD_TESTS=ON",
        "-DSILIGEN_USE_PCH=OFF",
        "-DSILIGEN_PARALLEL_COMPILE=ON",
        "--graphviz=$totalDotPath"
    )
    Write-Output "dependency graph (cmake): $($cmakeCommand.Source) $($configureArgs -join ' ')"
    & $cmakeCommand.Source @configureArgs
    $cmakeExitCode = $LASTEXITCODE
    if ($cmakeExitCode -ne 0) {
        if ($SoftFail) {
            $cmakeStatus = "soft-failed"
            $cmakeDetail = "cmake graphviz export failed with exit=$cmakeExitCode"
        }
        else {
            throw "CMake graphviz 导出失败，退出码: $cmakeExitCode"
        }
    }
    elseif (-not (Test-Path $totalDotPath)) {
        if ($SoftFail) {
            $cmakeStatus = "soft-failed"
            $cmakeDetail = "cmake graphviz dot file was not created"
        }
        else {
            throw "CMake graphviz dot file was not created: $totalDotPath"
        }
    }
    else {
        $cmakeStatus = "passed"
        $cmakeDetail = "workspace-targets.dot generated"
        foreach ($targetName in @(
            "siligen_runtime_service",
            "siligen_runtime_gateway",
            "siligen_planner_cli",
            "siligen_runtime_host_unit_tests",
            "siligen_dxf_geometry_unit_tests",
            "siligen_job_ingest_unit_tests"
        )) {
            $subsetPath = Join-Path $cmakeReportDir ("{0}.dot" -f $targetName)
            $created = Write-TargetSubsetGraph -SourceDotPath $totalDotPath -TargetName $targetName -OutputPath $subsetPath
            if (-not $created -and (Test-Path $subsetPath)) {
                Remove-Item -LiteralPath $subsetPath -Force
            }
        }
    }
}
$summaryLines += ('- cmake_status: `{0}`' -f $cmakeStatus)
$summaryLines += "- cmake_detail: $cmakeDetail"
$summaryLines += ''

$pydepsCommand = Resolve-WorkspaceToolPath -ToolNames @("pydeps") 
$dotCommand = Resolve-WorkspaceToolPath -ToolNames @("dot")
$pythonStatus = "skipped"
$pythonDetail = ""
if ([string]::IsNullOrWhiteSpace($pydepsCommand)) {
    if ($SoftFail) {
        $pythonStatus = "tool-missing"
        $pythonDetail = "pydeps not found"
    }
    else {
        throw "pydeps not found; dependency graph export is a blocking gate. Run .\scripts\validation\install-python-deps.ps1."
    }
}
else {
    $existingPythonPath = $env:PYTHONPATH
    $env:PYTHONPATH = ((@(
        (Join-Path $workspaceRoot "apps\hmi-app\src"),
        (Join-Path $workspaceRoot "modules\hmi-application\application"),
        (Join-Path $workspaceRoot "modules\dxf-geometry\application"),
        (Join-Path $workspaceRoot "shared\testing\test-kit\src"),
        $existingPythonPath
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }) -join [IO.Path]::PathSeparator)
    try {
        $pythonStatus = "passed"
        $pythonPackages = @(
            @{
                PackageName = "hmi_client"
                EntryPath = Join-Path $workspaceRoot "apps\hmi-app\src\hmi_client\__init__.py"
                OnlyPrefix = "hmi_client"
            },
            @{
                PackageName = "hmi_application"
                EntryPath = Join-Path $workspaceRoot "modules\hmi-application\application\hmi_application\__init__.py"
                OnlyPrefix = "hmi_application"
            },
            @{
                PackageName = "engineering_data"
                EntryPath = Join-Path $workspaceRoot "modules\dxf-geometry\application\engineering_data\__init__.py"
                OnlyPrefix = "engineering_data"
            },
            @{
                PackageName = "test_kit"
                EntryPath = Join-Path $workspaceRoot "shared\testing\test-kit\src\test_kit\__init__.py"
                OnlyPrefix = "test_kit"
            }
        )
        foreach ($package in $pythonPackages) {
            $packageName = [string]$package.PackageName
            $dotPath = Join-Path $pythonReportDir ("{0}.dot" -f $packageName)
            $depsPath = Join-Path $pythonReportDir ("{0}.deps.json" -f $packageName)
            $args = @(
                [string]$package.EntryPath,
                "--noshow",
                "--show-deps",
                "--deps-output",
                $depsPath,
                "--dot-output",
                $dotPath,
                "--no-output",
                "--only",
                [string]$package.OnlyPrefix
            )
            Write-Output "dependency graph (pydeps): $pydepsCommand $($args -join ' ')"
            & $pydepsCommand @args
            $pydepsExitCode = $LASTEXITCODE
            if ($pydepsExitCode -ne 0) {
                if ($SoftFail) {
                    $pythonStatus = "soft-failed"
                    $pythonDetail = "pydeps export failed for $packageName with exit=$pydepsExitCode"
                    break
                }
                else {
                    throw "pydeps export failed for $packageName with exit=$pydepsExitCode"
                }
            }

            if (-not [string]::IsNullOrWhiteSpace($dotCommand) -and (Test-Path $dotPath)) {
                $svgPath = Join-Path $pythonReportDir ("{0}.svg" -f $packageName)
                & $dotCommand -Tsvg $dotPath -o $svgPath | Out-Null
            }
        }
        if ($pythonStatus -eq "passed" -and [string]::IsNullOrWhiteSpace($dotCommand)) {
            $pythonDetail = "dot not found; generated .dot/.deps.json only"
        }
    }
    finally {
        $env:PYTHONPATH = $existingPythonPath
    }
}
$summaryLines += ('- python_status: `{0}`' -f $pythonStatus)
$summaryLines += "- python_detail: $pythonDetail"
$summaryLines += ''
$summaryLines += '- view_tip: Open `.dot` in Graphviz-compatible tools, or inspect generated `.svg` when `dot` is available.'

Set-Content -LiteralPath $summaryPath -Value ($summaryLines -join "`r`n") -Encoding UTF8

Write-Output "dependency-graph summary: $summaryPath"
Write-Output "dependency-graph cmake dir: $cmakeReportDir"
Write-Output "dependency-graph python dir: $pythonReportDir"

exit 0
