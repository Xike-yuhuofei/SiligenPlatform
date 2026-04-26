[CmdletBinding()]
param(
    [string]$ReportDir = "tests\reports\pre-push-gate\git-hygiene",
    [string[]]$ChangedFile = @(),
    [string]$ChangedFileListPath = "",
    [string]$BaseSha = "",
    [string]$HeadSha = "HEAD",
    [switch]$DirtyWorktree,
    [string]$DirtyPolicy = "unknown"
)

$ErrorActionPreference = "Stop"
$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent

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

function Get-ChangedFiles {
    $files = Expand-CommaSeparatedValues -Values $ChangedFile
    if (-not [string]::IsNullOrWhiteSpace($ChangedFileListPath)) {
        $path = Resolve-WorkspacePath -PathValue $ChangedFileListPath
        if (Test-Path -LiteralPath $path) {
            $files += @(Get-Content -Path $path | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
        }
    }
    return @($files | ForEach-Object { ([string]$_).Replace('\', '/') } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique)
}

function Add-Issue {
    param([System.Collections.Generic.List[object]]$Issues, [string]$Id, [string]$Severity, [string]$Path, [string]$Message, [string]$Hint)
    $Issues.Add([pscustomobject]@{
        id = $Id
        severity = $Severity
        path = $Path
        message = $Message
        hint = $Hint
    }) | Out-Null
}

function Test-JsonFile {
    param([string]$Path)
    Get-Content -Raw -Path $Path | ConvertFrom-Json | Out-Null
}

function Test-PowerShellFile {
    param([string]$Path)
    $tokens = $null
    $errors = $null
    [System.Management.Automation.Language.Parser]::ParseFile($Path, [ref]$tokens, [ref]$errors) | Out-Null
    if ($errors.Count -gt 0) {
        throw ($errors | ForEach-Object { $_.Message }) -join "; "
    }
}

$reportDirPath = Resolve-WorkspacePath -PathValue $ReportDir
New-Item -ItemType Directory -Force -Path $reportDirPath | Out-Null
$jsonPath = Join-Path $reportDirPath "pre-push-hygiene.json"
$mdPath = Join-Path $reportDirPath "pre-push-hygiene.md"

$issues = [System.Collections.Generic.List[object]]::new()
$changedFiles = Get-ChangedFiles

$unmerged = @(git diff --name-only --diff-filter=U)
if ($LASTEXITCODE -ne 0) {
    Add-Issue -Issues $issues -Id "git-unmerged-state-unavailable" -Severity "error" -Path "" -Message "Unable to inspect unresolved conflict state." -Hint "Run git status and resolve index errors before pushing."
} elseif ($unmerged.Count -gt 0) {
    foreach ($file in $unmerged) {
        Add-Issue -Issues $issues -Id "unresolved-conflict" -Severity "error" -Path $file -Message "Unresolved conflict remains." -Hint "Resolve conflict markers and stage a completed resolution."
    }
}

if ([string]::IsNullOrWhiteSpace($BaseSha)) {
    Add-Issue -Issues $issues -Id "missing-push-range" -Severity "error" -Path "" -Message "Pre-push commit range was not provided." -Hint "Run through invoke-pre-push-gate.ps1 so the hook input or upstream range is recorded."
}
if ($changedFiles.Count -eq 0) {
    Add-Issue -Issues $issues -Id "empty-change-set" -Severity "error" -Path "" -Message "No changed files were available for pre-push validation." -Hint "Confirm the upstream or pre-push hook input can identify the pending commit range."
}

$largeFileThresholdBytes = 5MB
$generatedPatterns = @(
    "^tests/reports/",
    "^build/",
    "^dist/",
    "^out/",
    "^coverage/",
    "^node_modules/",
    "^\\.pytest_cache/",
    "^__pycache__/",
    "\\.pyc$",
    "\\.pdb$",
    "\\.ilk$",
    "\\.obj$"
)
$pathPattern = '^[A-Za-z0-9._/\-]+$'
$secretPatterns = @(
    "AKIA[0-9A-Z]{16}",
    "(?i)(api[_-]?key|secret|token|password)\s*[:=]\s*['""][^'""]{8,}['""]",
    "-----BEGIN (RSA |EC |OPENSSH |DSA )?PRIVATE KEY-----",
    "sk-[A-Za-z0-9]{20,}"
)

foreach ($file in $changedFiles) {
    $normalized = $file.Replace('\', '/')
    if ($normalized -notmatch $pathPattern) {
        Add-Issue -Issues $issues -Id "invalid-path-name" -Severity "error" -Path $normalized -Message "Path contains characters outside the repository naming contract." -Hint "Use ASCII letters, digits, dot, slash, underscore, or dash."
    }
    foreach ($pattern in $generatedPatterns) {
        if ($normalized -match $pattern) {
            Add-Issue -Issues $issues -Id "generated-artifact" -Severity "error" -Path $normalized -Message "Generated output or report directory is in the push range." -Hint "Do not commit generated reports or build outputs."
            break
        }
    }

    $fullPath = Join-Path $workspaceRoot ($normalized -replace '/', [System.IO.Path]::DirectorySeparatorChar)
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) { continue }
    $item = Get-Item -LiteralPath $fullPath
    if ($item.Length -gt $largeFileThresholdBytes) {
        Add-Issue -Issues $issues -Id "large-file" -Severity "error" -Path $normalized -Message "File size $($item.Length) exceeds the 5 MiB pre-push threshold." -Hint "Use artifact storage or document an explicit large-file exception."
    }

    $extension = [System.IO.Path]::GetExtension($fullPath).ToLowerInvariant()
    $isTextCandidate = $extension -in @(".ps1", ".py", ".json", ".yaml", ".yml", ".md", ".txt", ".toml", ".ini", ".cfg", ".conf")
    if ($isTextCandidate) {
        $bytes = [System.IO.File]::ReadAllBytes($fullPath)
        if ($bytes.Length -gt 0 -and $bytes -contains 0) {
            Add-Issue -Issues $issues -Id "binary-text-candidate" -Severity "error" -Path $normalized -Message "Text-like file contains NUL bytes." -Hint "Check encoding or remove binary content."
            continue
        }
        $text = [System.IO.File]::ReadAllText($fullPath)
        if ($text -match "`r(?!`n)") {
            Add-Issue -Issues $issues -Id "invalid-newline" -Severity "error" -Path $normalized -Message "File contains bare CR newlines." -Hint "Use LF or CRLF consistently."
        }
        foreach ($secretPattern in $secretPatterns) {
            if ($text -match $secretPattern) {
                Add-Issue -Issues $issues -Id "secret-pattern" -Severity "error" -Path $normalized -Message "Potential secret or credential pattern detected." -Hint "Remove the secret and rotate it before pushing."
                break
            }
        }
    }

    try {
        switch ($extension) {
            ".json" { Test-JsonFile -Path $fullPath }
            ".ps1" { Test-PowerShellFile -Path $fullPath }
            ".py" { python -m py_compile $fullPath; if ($LASTEXITCODE -ne 0) { throw "python py_compile exited $LASTEXITCODE" } }
        }
    } catch {
        Add-Issue -Issues $issues -Id "parse-failed" -Severity "error" -Path $normalized -Message $_.Exception.Message -Hint "Fix syntax before pushing."
    }
}

$yamlFiles = @($changedFiles | Where-Object { $_ -match '\.(ya?ml)$' })
if ($yamlFiles.Count -gt 0) {
    $pythonYamlProbe = @'
import importlib.util
import pathlib
import sys
if importlib.util.find_spec("yaml") is None and importlib.util.find_spec("ruamel.yaml") is None:
    print("YAML parser missing. Install PyYAML or ruamel.yaml in the validation environment.")
    sys.exit(127)
use_pyyaml = importlib.util.find_spec("yaml") is not None
if use_pyyaml:
    import yaml
else:
    from ruamel.yaml import YAML
    yaml = YAML(typ="safe")
ok = True
root = pathlib.Path(sys.argv[1])
for rel in sys.argv[2:]:
    path = root / rel
    try:
        text = path.read_text(encoding="utf-8")
        if use_pyyaml:
            yaml.safe_load(text)
        else:
            yaml.load(text)
    except Exception as exc:
        ok = False
        print(f"{rel}: {exc}")
sys.exit(0 if ok else 1)
'@
    $yamlProbePath = Join-Path $reportDirPath "yaml-parse-probe.py"
    Set-Content -Path $yamlProbePath -Value $pythonYamlProbe -Encoding UTF8
    python $yamlProbePath $workspaceRoot @yamlFiles | Tee-Object -FilePath (Join-Path $reportDirPath "yaml-parse.log") | Out-Null
    if ($LASTEXITCODE -eq 127) {
        Add-Issue -Issues $issues -Id "yaml-parser-missing" -Severity "error" -Path "" -Message "YAML parser missing for changed YAML files." -Hint "Install PyYAML or ruamel.yaml; pre-push does not download tools at runtime."
    } elseif ($LASTEXITCODE -ne 0) {
        Add-Issue -Issues $issues -Id "yaml-parse-failed" -Severity "error" -Path "" -Message "One or more changed YAML files failed parsing." -Hint "See yaml-parse.log in the hygiene report."
    }
}

$status = if (($issues | Where-Object { $_.severity -eq "error" }).Count -eq 0) { "passed" } else { "failed" }
$summary = [ordered]@{
    schemaVersion = 1
    generated_at = (Get-Date).ToString("o")
    status = $status
    base_sha = $BaseSha
    head_sha = $HeadSha
    changed_files = @($changedFiles)
    dirty_worktree = $DirtyWorktree.IsPresent
    dirty_policy = $DirtyPolicy
    checks = @(
        "unresolved-conflict",
        "push-range-confirmation",
        "large-file",
        "secret-pattern",
        "generated-artifact",
        "path-name",
        "json-yaml-powershell-python-parse"
    )
    issues = @($issues)
}
$summary | ConvertTo-Json -Depth 12 | Set-Content -Path $jsonPath -Encoding UTF8

$lines = @("# Pre-push Hygiene", "", "- status: ``$status``", "- base_sha: ``$BaseSha``", "- head_sha: ``$HeadSha``", "- dirty_worktree: ``$($DirtyWorktree.IsPresent)``", "- dirty_policy: ``$DirtyPolicy``", "- changed_files: ``$($changedFiles.Count)``", "")
if ($issues.Count -eq 0) {
    $lines += "- no issues"
} else {
    $lines += "| Severity | Check | Path | Message | Hint |"
    $lines += "|---|---|---|---|---|"
    foreach ($issue in $issues) {
        $lines += "| $($issue.severity) | $($issue.id) | ``$($issue.path)`` | $($issue.message) | $($issue.hint) |"
    }
}
$lines | Set-Content -Path $mdPath -Encoding UTF8

Write-Output "pre-push hygiene=$status report=$reportDirPath"
if ($status -ne "passed") { exit 1 }
