[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Command,

    [switch]$Approve
)

$ErrorActionPreference = "Stop"

$rules = @(
    @{ Name = "git reset --hard"; Pattern = '(^|\s)git\s+reset\s+--hard(\s|$)'; Severity = "critical"; Hint = "会丢失未提交改动" },
    @{ Name = "git push --force"; Pattern = '(^|\s)git\s+push(\s+\S+)*\s+--force(\s|$)|(^|\s)git\s+push(\s+\S+)*\s+-f(\s|$)'; Severity = "critical"; Hint = "会覆盖远端历史" },
    @{ Name = "rm -rf"; Pattern = '(^|\s)rm\s+-rf(\s|$)|(^|\s)rm\s+-r(\s|$)|(^|\s)rm\s+--recursive(\s|$)'; Severity = "critical"; Hint = "可能造成不可逆删除" },
    @{ Name = "format drive"; Pattern = '(^|\s)format(\s|$)'; Severity = "critical"; Hint = "可能造成数据不可逆损坏" },
    @{ Name = "drop table/database"; Pattern = '\bDROP\s+(TABLE|DATABASE)\b'; Severity = "high"; Hint = "可能造成数据库数据丢失" }
)

$matches = @()
foreach ($rule in $rules) {
    if ($Command -match $rule.Pattern) {
        $matches += $rule
    }
}

if ($matches.Count -gt 0 -and -not $Approve) {
    Write-Host "BLOCKED: 检测到高风险命令，默认拒绝执行。" -ForegroundColor Red
    foreach ($m in $matches) {
        Write-Host ("- {0} [{1}]：{2}" -f $m.Name, $m.Severity, $m.Hint) -ForegroundColor Yellow
    }
    Write-Host "如确认执行，请补充 -Approve 并确保已有回滚方案。" -ForegroundColor Yellow
    exit 2
}

if ($matches.Count -gt 0 -and $Approve) {
    Write-Warning "高风险命令已显式批准执行。"
}

$shell = Get-Command pwsh -ErrorAction SilentlyContinue
if (-not $shell) {
    $shell = Get-Command powershell -ErrorAction SilentlyContinue
}

if (-not $shell) {
    Write-Error "未找到可用的 PowerShell 解释器（pwsh/powershell）。"
    exit 1
}

& $shell.Source -NoProfile -Command $Command
$exitCode = $LASTEXITCODE
exit $exitCode
