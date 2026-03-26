[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$PromptFile,

    [string]$Profile = "club",

    [string]$GitBashPath = "C:\Program Files\Git\bin\bash.exe",

    [string]$LogDir,

    [string]$Title = "codex-club",

    [switch]$NoKeepShell,

    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

function Get-UserCodexHome {
    if (-not [string]::IsNullOrWhiteSpace($env:CODEX_HOME)) {
        return $env:CODEX_HOME
    }

    return (Join-Path $HOME ".codex")
}

function Get-PropValue {
    param(
        [object]$Object,
        [string]$Name
    )

    if ($null -eq $Object) {
        return $null
    }

    if ($Object.PSObject.Properties.Name -contains $Name) {
        return $Object.$Name
    }

    return $null
}

function Convert-ToBashPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $unix = $fullPath -replace "\\", "/"

    if ($unix -match "^([A-Za-z]):/(.*)$") {
        return "/" + $Matches[1].ToLowerInvariant() + "/" + $Matches[2]
    }

    return $unix
}

function Get-WindowsTerminalDefaultStartingDirectory {
    $candidates = @(
        "$env:LOCALAPPDATA\Packages\Microsoft.WindowsTerminal_8wekyb3d8bbwe\LocalState\settings.json",
        "$env:LOCALAPPDATA\Microsoft\Windows Terminal\settings.json"
    )

    foreach ($settingsPath in $candidates) {
        if (-not (Test-Path $settingsPath)) {
            continue
        }

        try {
            $settings = Get-Content -Path $settingsPath -Raw | ConvertFrom-Json
        } catch {
            continue
        }

        $profiles = Get-PropValue -Object $settings -Name "profiles"
        $profileList = Get-PropValue -Object $profiles -Name "list"
        $profileDefaults = Get-PropValue -Object $profiles -Name "defaults"
        $defaultGuid = Get-PropValue -Object $settings -Name "defaultProfile"

        $profile = $null
        if (-not [string]::IsNullOrWhiteSpace($defaultGuid) -and $null -ne $profileList) {
            $profile = $profileList | Where-Object { $_.guid -eq $defaultGuid } | Select-Object -First 1
        }

        $dir = Get-PropValue -Object $profile -Name "startingDirectory"
        if ([string]::IsNullOrWhiteSpace($dir)) {
            $dir = Get-PropValue -Object $profileDefaults -Name "startingDirectory"
        }

        if (-not [string]::IsNullOrWhiteSpace($dir)) {
            return [Environment]::ExpandEnvironmentVariables($dir)
        }
    }

    return (Get-Location).Path
}

if (-not (Get-Command wt -ErrorAction SilentlyContinue)) {
    throw "未找到 wt.exe，请先安装/启用 Windows Terminal。"
}

if (-not (Test-Path $GitBashPath)) {
    throw "Git Bash 不存在：$GitBashPath"
}

$promptFileFull = [System.IO.Path]::GetFullPath($PromptFile)
if (-not (Test-Path $promptFileFull)) {
    throw "Prompt 文件不存在：$promptFileFull"
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if ([string]::IsNullOrWhiteSpace($LogDir)) {
    $LogDir = Join-Path $repoRoot "logs\codex-launch"
}

$notifyScriptWin = Join-Path $repoRoot "scripts\validation\invoke-codex-notify.ps1"
$notifyScriptBash = Convert-ToBashPath -Path $notifyScriptWin
$notifyConfigWin = Join-Path (Get-UserCodexHome) "notify.local.json"

$startDir = Get-WindowsTerminalDefaultStartingDirectory
if (-not (Test-Path $startDir)) {
    $startDir = (Get-Location).Path
}

New-Item -ItemType Directory -Path $LogDir -Force | Out-Null

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$task = [System.IO.Path]::GetFileNameWithoutExtension($promptFileFull)
$promptBash = Convert-ToBashPath -Path $promptFileFull
$logFile = Join-Path $LogDir ("codex-club-{0}-{1}.log" -f $task, $timestamp)
$logBash = Convert-ToBashPath -Path $logFile
$tempScriptWin = Join-Path ([System.IO.Path]::GetTempPath()) ("codex-club-{0}-{1}.sh" -f $task, $timestamp)
$tempScriptBash = Convert-ToBashPath -Path $tempScriptWin
$postAction = if ($NoKeepShell) { "exit \$RC" } else { "exec bash -li" }

$template = @'
#!/usr/bin/env bash
set +e
FILE="__PROMPT_FILE__"
LOG="__LOG_FILE__"
TASK="__TASK_NAME__"
NOTIFY_SCRIPT_WIN='__NOTIFY_SCRIPT_WIN__'
NOTIFY_SCRIPT_BASH="__NOTIFY_SCRIPT_BASH__"
NOTIFY_CONFIG_WIN='__NOTIFY_CONFIG_WIN__'
{
  echo "[INFO] task: $TASK"
  echo "[INFO] start: $(date '+%F %T')"
  echo "[INFO] pwd: $(pwd)"
  echo "[INFO] prompt_file: $FILE"
  command -v codex >/dev/null 2>&1 || { echo "[ERROR] codex not found in PATH"; exit 127; }
  [ -f "$FILE" ] || { echo "[ERROR] prompt file missing"; exit 2; }
} >>"$LOG" 2>&1
PROMPT_CONTENT="$(cat "$FILE")"
codex --yolo --profile __PROFILE__ "$PROMPT_CONTENT"
RC=$?
notify_codex_exit() {
  command -v powershell.exe >/dev/null 2>&1 || return 0
  [ -f "$NOTIFY_SCRIPT_BASH" ] || return 0
  powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass \
    -File "$NOTIFY_SCRIPT_WIN" \
    -ConfigFile "$NOTIFY_CONFIG_WIN" \
    -Task "$TASK" \
    -Profile "__PROFILE__" \
    -PromptFile "$FILE" \
    -LogFile "$LOG" \
    -ExitCode "$RC" >>"$LOG" 2>&1 || true
}
{
  echo "[INFO] codex_exit: $RC"
  echo "[INFO] end: $(date '+%F %T')"
} >>"$LOG" 2>&1
notify_codex_exit
if [ $RC -ne 0 ]; then
  echo "[ERROR] codex exited with code $RC"
  echo "[ERROR] log file: $LOG"
fi
__POST_ACTION__
'@

$scriptContent = $template.Replace("__PROMPT_FILE__", $promptBash)
$scriptContent = $scriptContent.Replace("__LOG_FILE__", $logBash)
$scriptContent = $scriptContent.Replace("__TASK_NAME__", $task)
$scriptContent = $scriptContent.Replace("__PROFILE__", $Profile)
$scriptContent = $scriptContent.Replace("__NOTIFY_SCRIPT_WIN__", $notifyScriptWin)
$scriptContent = $scriptContent.Replace("__NOTIFY_SCRIPT_BASH__", $notifyScriptBash)
$scriptContent = $scriptContent.Replace("__NOTIFY_CONFIG_WIN__", $notifyConfigWin)
$scriptContent = $scriptContent.Replace("__POST_ACTION__", $postAction)

Set-Content -Path $tempScriptWin -Value $scriptContent -Encoding utf8

$wtArgs = @(
    "-w", "0",
    "new-tab",
    "--title", $Title,
    "--startingDirectory", $startDir,
    $GitBashPath,
    "-li",
    $tempScriptBash
)

if ($DryRun) {
    Write-Output ("DRYRUN wt " + ($wtArgs -join " "))
} else {
    & wt @wtArgs | Out-Null
}

Write-Output ("START_DIR=" + $startDir)
Write-Output ("PROFILE=" + $Profile)
Write-Output ("PROMPT_FILE=" + $promptFileFull)
Write-Output ("LOG=" + $logFile)
Write-Output ("TEMP_SCRIPT=" + $tempScriptWin)
