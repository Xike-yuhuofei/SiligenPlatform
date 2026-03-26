param(
    [switch]$DryRun,
    [ValidateSet("online", "offline")]
    [string]$Mode = "online",
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$BuildConfig = "Debug",
    [string]$GatewayExe,
    [string]$GatewayLaunchSpec,
    [switch]$DisableGatewayAutostart,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$AppArgs
)

$ErrorActionPreference = "Stop"

$projectRoot = $PSScriptRoot
$runner = Join-Path $projectRoot "scripts\run.ps1"
$contractBuilder = Join-Path $projectRoot "scripts\new-gateway-launch-contract.ps1"
$defaultGatewaySpec = Join-Path $projectRoot "config\gateway-launch.json"

function Resolve-GeneratedContractPath {
    param(
        [object[]]$BuilderOutput,
        [string]$FailureMessage
    )

    $candidatePath = [string]($BuilderOutput | Select-Object -Last 1)
    if ([string]::IsNullOrWhiteSpace($candidatePath)) {
        throw $FailureMessage
    }

    $resolvedCandidatePath = [System.IO.Path]::GetFullPath($candidatePath)
    if (-not (Test-Path $resolvedCandidatePath)) {
        throw "$FailureMessage Generated contract not found: $resolvedCandidatePath"
    }

    return $resolvedCandidatePath
}

function Resolve-OfficialGatewayContract {
    param(
        [string]$ExplicitGatewayLaunchSpec,
        [string]$ExplicitGatewayExe,
        [string]$DefaultGatewaySpecPath,
        [string]$ContractBuilderPath,
        [string]$BuildConfiguration,
        [string]$LaunchMode,
        [bool]$AutoStartDisabled,
        [switch]$DryRunMode
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitGatewayLaunchSpec)) {
        $resolvedExplicitSpec = [System.IO.Path]::GetFullPath($ExplicitGatewayLaunchSpec)
        if (-not (Test-Path $resolvedExplicitSpec)) {
            throw "Gateway launch contract not found: $resolvedExplicitSpec"
        }
        return @{
            Source = "explicit-spec"
            Path = $resolvedExplicitSpec
            Temporary = $false
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($ExplicitGatewayExe)) {
        if (-not (Test-Path $ContractBuilderPath)) {
            throw "Gateway contract builder not found: $ContractBuilderPath"
        }

        if ($DryRunMode) {
            $builderOutput = & $ContractBuilderPath -GatewayExe $ExplicitGatewayExe -BuildConfig $BuildConfiguration -DryRun
            return @{
                Source = "generated-from-explicit-exe"
                Path = ""
                Temporary = $true
                DryRunOutput = @($builderOutput)
            }
        }

        $generatedPath = & $ContractBuilderPath -GatewayExe $ExplicitGatewayExe -BuildConfig $BuildConfiguration

        return @{
            Source = "generated-from-explicit-exe"
            Path = Resolve-GeneratedContractPath `
                -BuilderOutput @($generatedPath) `
                -FailureMessage "Failed to generate gateway launch contract from explicit gateway executable."
            Temporary = $true
        }
    }

    $canGenerateDevContract = (-not $AutoStartDisabled) -and ($LaunchMode -eq "online")
    if ($canGenerateDevContract) {
        if (-not (Test-Path $ContractBuilderPath)) {
            throw "Gateway contract builder not found: $ContractBuilderPath"
        }

        if ($DryRunMode) {
            $builderOutput = & $ContractBuilderPath -BuildConfig $BuildConfiguration -DryRun
            return @{
                Source = "generated-dev"
                Path = ""
                Temporary = $true
                DryRunOutput = @($builderOutput)
            }
        }

        $generatedContractPath = & $ContractBuilderPath -BuildConfig $BuildConfiguration

        return @{
            Source = "generated-dev"
            Path = Resolve-GeneratedContractPath `
                -BuilderOutput @($generatedContractPath) `
                -FailureMessage "Failed to generate development gateway launch contract."
            Temporary = $true
        }
    }

    if (Test-Path $DefaultGatewaySpecPath) {
        return @{
            Source = "app-default"
            Path = [System.IO.Path]::GetFullPath($DefaultGatewaySpecPath)
            Temporary = $false
        }
    }

    return $null
}

if (-not (Test-Path $runner)) {
    Write-Error "Canonical hmi-app run script not found: $runner"
}

$resolvedGatewayContract = Resolve-OfficialGatewayContract `
    -ExplicitGatewayLaunchSpec $GatewayLaunchSpec `
    -ExplicitGatewayExe $GatewayExe `
    -DefaultGatewaySpecPath $defaultGatewaySpec `
    -ContractBuilderPath $contractBuilder `
    -BuildConfiguration $BuildConfig `
    -LaunchMode $Mode `
    -AutoStartDisabled:$DisableGatewayAutostart `
    -DryRunMode:$DryRun

if ($DryRun) {
    Write-Output "hmi-app target: $runner"
    Write-Output "mode: $Mode"
    Write-Output "build config: $BuildConfig"
    if ($resolvedGatewayContract) {
        Write-Output "gateway contract source: $($resolvedGatewayContract.Source)"
        if ($resolvedGatewayContract.Path) {
            Write-Output "gateway contract path: $($resolvedGatewayContract.Path)"
        }
        if ($resolvedGatewayContract.ContainsKey("DryRunOutput")) {
            foreach ($line in $resolvedGatewayContract.DryRunOutput) {
                Write-Output $line
            }
        }
    } else {
        Write-Output "gateway contract source: none"
    }
    if ($DisableGatewayAutostart) {
        Write-Output "gateway autostart: disabled"
    }
    exit 0
}

$forwardArgs = @()
$hasExplicitModeArg = $false
foreach ($arg in $AppArgs) {
    if ($arg -eq "--mode") {
        $hasExplicitModeArg = $true
        break
    }
}
if (-not $hasExplicitModeArg) {
    $forwardArgs += @("--mode", $Mode)
}
if ($AppArgs) {
    $forwardArgs += $AppArgs
}

$generatedContractPath = $null
$previousLaunchSpec = $env:SILIGEN_GATEWAY_LAUNCH_SPEC
$previousGatewayExe = $env:SILIGEN_GATEWAY_EXE
$previousOfficialEntrypoint = $env:SILIGEN_HMI_OFFICIAL_ENTRYPOINT
$previousAutoStart = $env:SILIGEN_GATEWAY_AUTOSTART

try {
    if ($resolvedGatewayContract -and -not [string]::IsNullOrWhiteSpace($resolvedGatewayContract.Path)) {
        $env:SILIGEN_GATEWAY_LAUNCH_SPEC = [System.IO.Path]::GetFullPath($resolvedGatewayContract.Path)
        Remove-Item Env:SILIGEN_GATEWAY_EXE -ErrorAction SilentlyContinue
        if ($resolvedGatewayContract.Temporary) {
            $generatedContractPath = $env:SILIGEN_GATEWAY_LAUNCH_SPEC
        }
    }

    $env:SILIGEN_HMI_OFFICIAL_ENTRYPOINT = "1"

    if ($DisableGatewayAutostart) {
        $env:SILIGEN_GATEWAY_AUTOSTART = "0"
    }

    & $runner @forwardArgs
    exit $LASTEXITCODE
} finally {
    if ([string]::IsNullOrWhiteSpace($previousLaunchSpec)) {
        Remove-Item Env:SILIGEN_GATEWAY_LAUNCH_SPEC -ErrorAction SilentlyContinue
    } else {
        $env:SILIGEN_GATEWAY_LAUNCH_SPEC = $previousLaunchSpec
    }

    if ([string]::IsNullOrWhiteSpace($previousGatewayExe)) {
        Remove-Item Env:SILIGEN_GATEWAY_EXE -ErrorAction SilentlyContinue
    } else {
        $env:SILIGEN_GATEWAY_EXE = $previousGatewayExe
    }

    if ([string]::IsNullOrWhiteSpace($previousOfficialEntrypoint)) {
        Remove-Item Env:SILIGEN_HMI_OFFICIAL_ENTRYPOINT -ErrorAction SilentlyContinue
    } else {
        $env:SILIGEN_HMI_OFFICIAL_ENTRYPOINT = $previousOfficialEntrypoint
    }

    if ([string]::IsNullOrWhiteSpace($previousAutoStart)) {
        Remove-Item Env:SILIGEN_GATEWAY_AUTOSTART -ErrorAction SilentlyContinue
    } else {
        $env:SILIGEN_GATEWAY_AUTOSTART = $previousAutoStart
    }

    if (-not [string]::IsNullOrWhiteSpace($generatedContractPath) -and (Test-Path $generatedContractPath)) {
        Remove-Item $generatedContractPath -Force -ErrorAction SilentlyContinue
    }
}
