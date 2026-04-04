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

function Get-DefaultStartingDirectory {
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

function Get-GitBashLauncherPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    if (-not (Test-Path $fullPath)) {
        throw "Git Bash 不存在：$fullPath"
    }

    if ([System.IO.Path]::GetFileName($fullPath).Equals("bash.exe", [System.StringComparison]::OrdinalIgnoreCase)) {
        $gitBashPath = Join-Path (Split-Path (Split-Path $fullPath -Parent) -Parent) "git-bash.exe"
        if (Test-Path $gitBashPath) {
            return [System.IO.Path]::GetFullPath($gitBashPath)
        }
    }

    return $fullPath
}

$gitBashLauncherPath = Get-GitBashLauncherPath -Path $GitBashPath

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

$startDir = Get-DefaultStartingDirectory
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
START_TS=$(date +%s)
HEARTBEAT_PID=""
log_info() {
  echo "$1" | tee -a "$LOG"
}
log_error() {
  echo "$1" | tee -a "$LOG" >&2
}
log_banner() {
  log_info "============================================================"
  log_info "$1"
  log_info "============================================================"
}
start_heartbeat() {
  (
    while true; do
      sleep 30
      NOW_TS=$(date +%s)
      ELAPSED=$((NOW_TS - START_TS))
      log_info "[INFO] heartbeat: task still running (${ELAPSED}s elapsed)"
    done
  ) &
  HEARTBEAT_PID=$!
}
stop_heartbeat() {
  if [ -n "$HEARTBEAT_PID" ] && kill -0 "$HEARTBEAT_PID" 2>/dev/null; then
    kill "$HEARTBEAT_PID" 2>/dev/null || true
    wait "$HEARTBEAT_PID" 2>/dev/null || true
  fi
}
{
  log_banner "[INFO] codex task bootstrap: $TASK"
  log_info "[INFO] task: $TASK"
  log_info "[INFO] start: $(date '+%F %T')"
  log_info "[INFO] pwd: $(pwd)"
  log_info "[INFO] prompt_file: $FILE"
  log_info "[INFO] log_file: $LOG"
  log_info "[INFO] phase: validating-environment"
  command -v codex >/dev/null 2>&1 || { log_error "[ERROR] codex not found in PATH"; exit 127; }
  [ -f "$FILE" ] || { log_error "[ERROR] prompt file missing"; exit 2; }
  log_info "[INFO] phase: loading-prompt"
}
PROMPT_CONTENT="$(cat "$FILE")"
log_info "[INFO] prompt_bytes: $(wc -c < "$FILE")"
log_info "[INFO] phase: launching-codex"
start_heartbeat
codex --yolo --profile __PROFILE__ "$PROMPT_CONTENT" 2>&1 | tee -a "$LOG"
RC=${PIPESTATUS[0]}
stop_heartbeat
END_TS=$(date +%s)
ELAPSED=$((END_TS - START_TS))
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
  log_info "[INFO] phase: notifying"
  log_info "[INFO] codex_exit: $RC"
  log_info "[INFO] elapsed_seconds: $ELAPSED"
  log_info "[INFO] end: $(date '+%F %T')"
}
notify_codex_exit
if [ $RC -ne 0 ]; then
  log_banner "[ERROR] codex task failed: $TASK"
  log_error "[ERROR] codex exited with code $RC"
  log_error "[ERROR] log file: $LOG"
else
  log_banner "[INFO] codex task completed: $TASK (${ELAPSED}s)"
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

$processArgs = @(
    "--cd=$startDir",
    "-li",
    $tempScriptBash
)

if ($DryRun) {
    Write-Output ("DRYRUN Start-Process " + $gitBashLauncherPath + " " + ($processArgs -join " "))
} else {
    Start-Process -FilePath $gitBashLauncherPath -ArgumentList $processArgs -WorkingDirectory $startDir | Out-Null
}

Write-Output ("START_DIR=" + $startDir)
Write-Output ("PROFILE=" + $Profile)
Write-Output ("TITLE=" + $Title)
Write-Output ("GIT_BASH_LAUNCHER=" + $gitBashLauncherPath)
Write-Output ("PROMPT_FILE=" + $promptFileFull)
Write-Output ("LOG=" + $logFile)
Write-Output ("TEMP_SCRIPT=" + $tempScriptWin)
