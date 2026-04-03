[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$PromptFile,

    [string]$WorkingRoot,

    [string]$Profile = "codexmanager",

    [string]$Title = "codex-fresh",

    [ValidateSet("Tab", "Window")]
    [string]$LaunchMode = "Tab",

    [string]$GitBashPath,

    [switch]$NoKeepShell,

    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

function Resolve-WorkspaceRoot {
    param(
        [string]$Path
    )

    $candidate = if ([string]::IsNullOrWhiteSpace($Path)) {
        (Get-Location).Path
    } else {
        [System.IO.Path]::GetFullPath($Path)
    }

    if (-not (Test-Path -LiteralPath $candidate)) {
        throw "WorkingRoot 不存在：$candidate"
    }

    $git = Get-Command git -ErrorAction SilentlyContinue
    if ($null -ne $git) {
        $resolved = & git -C $candidate rev-parse --show-toplevel 2>$null
        if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($resolved)) {
            return $resolved.Trim()
        }
    }

    return (Resolve-Path -LiteralPath $candidate).Path
}

function Escape-BashSingleQuoted {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Value
    )

    $quoteEscape = [string]([char]39) + [string]([char]34) + [string]([char]39) + [string]([char]34) + [string]([char]39)
    return $Value.Replace([string][char]39, $quoteEscape)
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

function Convert-ToBashPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    return ([System.IO.Path]::GetFullPath($Path)) -replace '\\', '/'
}

function Resolve-GitBashExecutable {
    param(
        [string]$OverridePath
    )

    $candidates = New-Object System.Collections.Generic.List[string]

    if (-not [string]::IsNullOrWhiteSpace($OverridePath)) {
        $candidates.Add([System.IO.Path]::GetFullPath($OverridePath))
    }

    $git = Get-Command git.exe -ErrorAction SilentlyContinue
    if ($null -ne $git) {
        $gitRoot = Split-Path -Parent (Split-Path -Parent $git.Source)
        $candidates.Add((Join-Path $gitRoot "bin\bash.exe"))
        $candidates.Add((Join-Path $gitRoot "git-bash.exe"))
    }

    $bash = Get-Command bash.exe -ErrorAction SilentlyContinue
    if ($null -ne $bash) {
        $candidates.Add($bash.Source)
    }

    $candidates.Add("C:\Program Files\Git\bin\bash.exe")
    $candidates.Add("C:\Program Files\Git\git-bash.exe")
    $candidates.Add("C:\Program Files (x86)\Git\bin\bash.exe")
    $candidates.Add("C:\Program Files (x86)\Git\git-bash.exe")

    foreach ($candidate in $candidates) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path -LiteralPath $candidate)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    throw "未找到 Git Bash，请安装 Git for Windows，或通过 -GitBashPath 显式指定 bash.exe。"
}

if (-not (Get-Command wt -ErrorAction SilentlyContinue)) {
    throw "未找到 wt.exe，请先安装或启用 Windows Terminal。"
}

$promptFileFull = [System.IO.Path]::GetFullPath($PromptFile)
if (-not (Test-Path -LiteralPath $promptFileFull)) {
    throw "交接文档不存在：$promptFileFull"
}

$workspaceRoot = Resolve-WorkspaceRoot -Path $WorkingRoot
$logDir = Join-Path $workspaceRoot "logs\codex-launch"
New-Item -ItemType Directory -Path $logDir -Force | Out-Null

$gitBashExe = Resolve-GitBashExecutable -OverridePath $GitBashPath

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$runId = [guid]::NewGuid().ToString("N").Substring(0, 8)
$task = [System.IO.Path]::GetFileNameWithoutExtension($promptFileFull)
$logFile = Join-Path $logDir ("fresh-session-{0}-{1}-{2}.log" -f $task, $timestamp, $runId)
$tempScript = Join-Path ([System.IO.Path]::GetTempPath()) ("fresh-session-{0}-{1}-{2}.sh" -f $task, $timestamp, $runId)
$effectiveProfile = $null

if (-not [string]::IsNullOrWhiteSpace($Profile)) {
    if (Test-CodexProfileExists -Name $Profile) {
        $effectiveProfile = $Profile
    } else {
        Write-Warning "Codex profile '$Profile' 不存在，改为不传 --profile。"
    }
}

$workspaceRootBash = Convert-ToBashPath -Path $workspaceRoot
$promptFileBash = Convert-ToBashPath -Path $promptFileFull
$logFileBash = Convert-ToBashPath -Path $logFile
$tempScriptBash = Convert-ToBashPath -Path $tempScript
$workspaceRootBashEscaped = Escape-BashSingleQuoted -Value $workspaceRootBash
$promptFileBashEscaped = Escape-BashSingleQuoted -Value $promptFileBash
$logFileBashEscaped = Escape-BashSingleQuoted -Value $logFileBash
$tempScriptBashEscapedForTemplate = Escape-BashSingleQuoted -Value $tempScriptBash
$profileBashEscaped = if ([string]::IsNullOrWhiteSpace($effectiveProfile)) {
    ""
} else {
    Escape-BashSingleQuoted -Value $effectiveProfile
}
$keepShellValue = if ($NoKeepShell) { "0" } else { "1" }
$launchTarget = if ($LaunchMode -eq "Tab" -and -not [string]::IsNullOrWhiteSpace($env:WT_SESSION)) { "0" } else { "new" }
$launchResolved = if ($launchTarget -eq "0") { "Tab" } else { "Window" }

$template = @'
#!/usr/bin/env bash
set -uo pipefail

workspace_root='__WORKING_ROOT__'
prompt_file='__PROMPT_FILE__'
log_file='__LOG_FILE__'
script_path='__SCRIPT_PATH__'
profile='__PROFILE__'
keep_shell='__KEEP_SHELL__'

cd "$workspace_root" || {
    printf '[ERROR] failed_to_cd=%s\n' "$workspace_root" >> "$log_file"
    exit 1
}

start_time=$(date '+%Y-%m-%d %H:%M:%S')
{
    printf '[INFO] start=%s\n' "$start_time"
    printf '[INFO] working_root=%s\n' "$workspace_root"
    printf '[INFO] prompt_file=%s\n' "$prompt_file"
    printf '[INFO] profile=%s\n' "$profile"
} >> "$log_file"

if ! command -v codex >/dev/null 2>&1; then
    echo 'codex 不在 PATH 中。'
    printf '[ERROR] codex_not_found=1\n' >> "$log_file"
    if [ "$keep_shell" = '1' ]; then
        exec bash --login -i
    fi
    exit 1
fi

if [ ! -f "$prompt_file" ]; then
    echo "交接文档不存在：$prompt_file"
    printf '[ERROR] prompt_file_missing=1\n' >> "$log_file"
    if [ "$keep_shell" = '1' ]; then
        exec bash --login -i
    fi
    exit 1
fi

startup_prompt=$(cat <<EOF
先读取交接文档：$prompt_file
工作根目录固定为：$workspace_root
不要 resume 或 fork 旧会话。
把交接文档中的“已确认事实”和“未完成事项”视为当前真值来源，并从“建议新窗口立刻执行的下一步”开始推进。
EOF
)

echo "[fresh-session] worktree: $workspace_root"
echo "[fresh-session] handoff:  $prompt_file"
echo "[fresh-session] profile:  ${profile:-'(none)'}"
echo "[fresh-session] log:      $log_file"

codex_args=(-C "$workspace_root")
if [ -n "$profile" ]; then
    codex_args+=(--profile "$profile")
fi

codex "${codex_args[@]}" "$startup_prompt"
exit_code=$?

end_time=$(date '+%Y-%m-%d %H:%M:%S')
{
    printf '[INFO] end=%s\n' "$end_time"
    printf '[INFO] exit_code=%s\n' "$exit_code"
} >> "$log_file"

if [ "$exit_code" -ne 0 ]; then
    echo "[fresh-session] codex exited with code $exit_code"
fi

if [ "$exit_code" -eq 0 ]; then
    rm -f "$script_path"
fi

if [ "$keep_shell" = '1' ]; then
    exec bash --login -i
fi

exit "$exit_code"
'@

$scriptContent = $template.Replace("__WORKING_ROOT__", $workspaceRootBashEscaped)
$scriptContent = $scriptContent.Replace("__PROMPT_FILE__", $promptFileBashEscaped)
$scriptContent = $scriptContent.Replace("__LOG_FILE__", $logFileBashEscaped)
$scriptContent = $scriptContent.Replace("__SCRIPT_PATH__", $tempScriptBashEscapedForTemplate)
$scriptContent = $scriptContent.Replace("__PROFILE__", $profileBashEscaped)
$scriptContent = $scriptContent.Replace("__KEEP_SHELL__", $keepShellValue)

Set-Content -Path $tempScript -Value $scriptContent -Encoding utf8

$bashCommand = "source '$tempScriptBashEscapedForTemplate'"
$wtArgs = @(
    "-w", $launchTarget,
    "new-tab",
    "--title", $Title,
    "--startingDirectory", $workspaceRoot,
    $gitBashExe,
    "--login",
    "-c", $bashCommand
)

if ($DryRun) {
    Write-Output ("DRYRUN wt " + ($wtArgs -join " "))
} else {
    & wt @wtArgs | Out-Null
}

Write-Output ("WORKING_ROOT=" + $workspaceRoot)
Write-Output ("PROMPT_FILE=" + $promptFileFull)
Write-Output ("LOG=" + $logFile)
Write-Output ("TEMP_SCRIPT=" + $tempScript)
Write-Output ("BASH=" + $gitBashExe)
Write-Output ("PROFILE=" + $(if ([string]::IsNullOrWhiteSpace($effectiveProfile)) { "" } else { $effectiveProfile }))
Write-Output ("LAUNCH_MODE_REQUESTED=" + $LaunchMode)
Write-Output ("LAUNCH_MODE_RESOLVED=" + $launchResolved)
