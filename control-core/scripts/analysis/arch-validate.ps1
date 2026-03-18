<#
.SYNOPSIS
    Validate hexagonal layer dependencies by scanning #include directives.

.DESCRIPTION
    Checks source files under the specified directory for illegal cross-layer
    include dependencies based on the project's hexagonal architecture rules.
#>

param(
    [string]$SourceDir = "src",
    [switch]$ReportOnly,
    [string[]]$ExcludePaths = @()
)

$extensions = @(".h", ".hpp", ".hh", ".hxx", ".cpp", ".cc", ".cxx", ".inl")

$resolvedSourceDir = $SourceDir
if (-not [System.IO.Path]::IsPathRooted($SourceDir)) {
    $repoRoot = Join-Path $PSScriptRoot "..\\.."
    $resolvedSourceDir = Join-Path (Resolve-Path -Path $repoRoot) $SourceDir
}

if (-not (Test-Path -Path $resolvedSourceDir)) {
    Write-Host "SourceDir not found: $resolvedSourceDir"
    exit 2
}

$files = Get-ChildItem -Path $resolvedSourceDir -Recurse -File -ErrorAction SilentlyContinue
Write-Host "Scanning $($files.Count) files under $resolvedSourceDir"

$violations = @()

foreach ($file in $files) {
    $fullPath = $file.FullName
    $extension = $file.Extension.ToLowerInvariant()
    if (-not ($extensions -contains $extension)) {
        continue
    }

    if ($ExcludePaths.Count -gt 0) {
        $skip = $false
        foreach ($pattern in $ExcludePaths) {
            if ($fullPath -match $pattern) {
                $skip = $true
                break
            }
        }
        if ($skip) {
            continue
        }
    }

    $layer = $null
    $forbidden = @()

    if ($fullPath -like "*\domain\*") {
        $layer = "domain"
        $forbidden = @("application/", "infrastructure/", "adapters/")
    } elseif ($fullPath -like "*\application\*") {
        $layer = "application"
        $forbidden = @("infrastructure/", "adapters/")
    } elseif ($fullPath -like "*\infrastructure\*") {
        $layer = "infrastructure"
        $forbidden = @("application/", "adapters/")
    } elseif ($fullPath -like "*\adapters\*") {
        $layer = "adapters"
        $forbidden = @("infrastructure/")
    } else {
        continue
    }

    $includeMatches = Select-String -Path $fullPath -Pattern '^\s*#\s*include\s+[<"]([^">]+)[">]' -AllMatches -ErrorAction SilentlyContinue
    foreach ($match in $includeMatches) {
        if ($match.Matches.Count -eq 0) {
            continue
        }
        $includePath = $match.Matches[0].Groups[1].Value.Replace("\\", "/")
        $normalizedInclude = $includePath -replace '^[.\\/]+', ''
        foreach ($pattern in $forbidden) {
            if ($normalizedInclude.StartsWith($pattern, [System.StringComparison]::OrdinalIgnoreCase)) {
                $relativePath = $fullPath
                try {
                    $relativePath = Resolve-Path -LiteralPath $fullPath -Relative
                } catch {
                    # Fallback to full path when relative path is unavailable.
                }

                $violations += [PSCustomObject]@{
                    File = $relativePath
                    Line = $match.LineNumber
                    Layer = $layer
                    Include = $includePath
                    Rule = "$layer -> $pattern"
                }
                break
            }
        }
    }
}

if ($violations.Count -gt 0) {
    Write-Host "Found $($violations.Count) architecture include violations."
    $violations | Sort-Object File, Line | ForEach-Object {
        "{0}:{1} [{2}] {3}" -f $_.File, $_.Line, $_.Layer, $_.Include
    }
    if (-not $ReportOnly) {
        exit 1
    }
} else {
    Write-Host "No architecture include violations found."
}

exit 0
