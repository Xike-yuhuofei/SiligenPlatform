[CmdletBinding()]
param(
    [string]$ConfigFile,

    [string]$Task,

    [string]$Profile,

    [string]$PromptFile,

    [string]$LogFile,

    [int]$ExitCode = 0,

    [string]$Title,

    [string]$Message
)

$ErrorActionPreference = "Stop"

function Get-UserCodexHome {
    if (-not [string]::IsNullOrWhiteSpace($env:CODEX_HOME)) {
        return $env:CODEX_HOME
    }

    return (Join-Path $HOME ".codex")
}

function Get-OptionalProperty {
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

function ConvertTo-NullableBoolean {
    param([string]$Value)

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return $null
    }

    switch ($Value.Trim().ToLowerInvariant()) {
        "1" { return $true }
        "true" { return $true }
        "yes" { return $true }
        "on" { return $true }
        "0" { return $false }
        "false" { return $false }
        "no" { return $false }
        "off" { return $false }
        default { throw "无法解析布尔值: $Value" }
    }
}

function Get-NotifyConfig {
    param([string]$Path)

    $config = [ordered]@{
        Enabled       = $false
        Provider      = "xtuis"
        XtuisEndpoint = "https://wx.xtuis.cn"
        XtuisToken    = $null
    }

    if (Test-Path $Path) {
        $raw = Get-Content -Path $Path -Raw
        if (-not [string]::IsNullOrWhiteSpace($raw)) {
            $json = $raw | ConvertFrom-Json
            $enabledValue = Get-OptionalProperty -Object $json -Name "enabled"
            if ($null -ne $enabledValue) {
                $config.Enabled = [bool]$enabledValue
            }

            $providerValue = Get-OptionalProperty -Object $json -Name "provider"
            if (-not [string]::IsNullOrWhiteSpace($providerValue)) {
                $config.Provider = $providerValue
            }

            $xtuis = Get-OptionalProperty -Object $json -Name "xtuis"
            if ($null -ne $xtuis) {
                $endpointValue = Get-OptionalProperty -Object $xtuis -Name "endpoint"
                $tokenValue = Get-OptionalProperty -Object $xtuis -Name "token"
                if (-not [string]::IsNullOrWhiteSpace($endpointValue)) {
                    $config.XtuisEndpoint = $endpointValue
                }
                if (-not [string]::IsNullOrWhiteSpace($tokenValue)) {
                    $config.XtuisToken = $tokenValue
                }
            }
        }
    }

    $envEnabled = ConvertTo-NullableBoolean -Value $env:CODEX_NOTIFY_ENABLED
    if ($null -ne $envEnabled) {
        $config.Enabled = $envEnabled
    }

    if (-not [string]::IsNullOrWhiteSpace($env:CODEX_NOTIFY_PROVIDER)) {
        $config.Provider = $env:CODEX_NOTIFY_PROVIDER
    }

    if (-not [string]::IsNullOrWhiteSpace($env:CODEX_NOTIFY_XTUIS_ENDPOINT)) {
        $config.XtuisEndpoint = $env:CODEX_NOTIFY_XTUIS_ENDPOINT
    }

    if (-not [string]::IsNullOrWhiteSpace($env:CODEX_NOTIFY_XTUIS_TOKEN)) {
        $config.XtuisToken = $env:CODEX_NOTIFY_XTUIS_TOKEN
    }

    return [PSCustomObject]$config
}

function Send-XtuisNotification {
    param(
        [string]$Endpoint,
        [string]$Token,
        [string]$NotificationTitle,
        [string]$NotificationMessage
    )

    if ([string]::IsNullOrWhiteSpace($Token)) {
        Write-Output "NOTIFY_SKIPPED provider=xtuis reason=missing_token"
        return
    }

    $normalizedEndpoint = $Endpoint.TrimEnd("/")
    $baseUri = "{0}/{1}.send" -f $normalizedEndpoint, $Token
    $uri = "{0}?text={1}&desp={2}" -f `
        $baseUri, `
        [System.Uri]::EscapeDataString($NotificationTitle), `
        [System.Uri]::EscapeDataString($NotificationMessage)

    try {
        $response = Invoke-WebRequest -Uri $uri -Method Get -UseBasicParsing -TimeoutSec 15
        if ($response.Content -is [System.Array]) {
            $responseText = [System.Text.Encoding]::UTF8.GetString([byte[]]$response.Content)
        } else {
            $responseText = [string]$response.Content
        }
        Write-Output ("NOTIFY_SENT provider=xtuis exit_code={0} response={1}" -f $ExitCode, $responseText)
    } catch {
        Write-Warning ("虾推啥通知发送失败: " + $_.Exception.Message)
    }
}

if ([string]::IsNullOrWhiteSpace($ConfigFile)) {
    $ConfigFile = Join-Path (Get-UserCodexHome) "notify.local.json"
}

$config = Get-NotifyConfig -Path $ConfigFile
if (-not $config.Enabled) {
    Write-Output "NOTIFY_SKIPPED reason=disabled"
    exit 0
}

$taskLabel = if (-not [string]::IsNullOrWhiteSpace($Task)) {
    $Task
} elseif (-not [string]::IsNullOrWhiteSpace($PromptFile)) {
    [System.IO.Path]::GetFileNameWithoutExtension($PromptFile)
} else {
    "codex-task"
}

$statusLabel = if ($ExitCode -eq 0) { "成功" } else { "失败" }

if ([string]::IsNullOrWhiteSpace($Title)) {
    $Title = "Codex{0} | {1}" -f $statusLabel, $taskLabel
}

if ([string]::IsNullOrWhiteSpace($Message)) {
    $lines = @(
        "任务: $taskLabel",
        "状态: $statusLabel",
        "退出码: $ExitCode"
    )

    if (-not [string]::IsNullOrWhiteSpace($Profile)) {
        $lines += "Profile: $Profile"
    }

    if (-not [string]::IsNullOrWhiteSpace($PromptFile)) {
        $lines += "Prompt: $PromptFile"
    }

    if (-not [string]::IsNullOrWhiteSpace($LogFile)) {
        $lines += "Log: $LogFile"
    }

    $lines += "时间: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
    $lines += "主机: $env:COMPUTERNAME"
    $Message = $lines -join "`n"
}

switch ($config.Provider.Trim().ToLowerInvariant()) {
    "xtuis" {
        Send-XtuisNotification `
            -Endpoint $config.XtuisEndpoint `
            -Token $config.XtuisToken `
            -NotificationTitle $Title `
            -NotificationMessage $Message
    }
    default {
        Write-Output ("NOTIFY_SKIPPED reason=unsupported_provider provider={0}" -f $config.Provider)
    }
}
