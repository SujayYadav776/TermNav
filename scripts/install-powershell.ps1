$ErrorActionPreference = 'Stop'
$launcher = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\termnav.ps1'))
if (-not (Test-Path -LiteralPath $launcher -PathType Leaf)) {
    throw "TermNav launcher not found: $launcher"
}

$profilePath = $PROFILE.CurrentUserAllHosts
$profileDirectory = Split-Path -Parent $profilePath
New-Item -ItemType Directory -Force -Path $profileDirectory | Out-Null
$profileText = if (Test-Path -LiteralPath $profilePath) { Get-Content -Raw -LiteralPath $profilePath } else { '' }
$profileText = [regex]::Replace($profileText, '(?ms)^# >>> TermNav launcher >>>\r?\n.*?^# <<< TermNav launcher <<<\r?\n?', '')
$literalLauncher = $launcher.Replace("'", "''")
$profileText = $profileText.TrimEnd() + "`r`n`r`n# >>> TermNav launcher >>>`r`nfunction global:termnav { & '$literalLauncher' @args }`r`n# <<< TermNav launcher <<<`r`n"
[System.IO.File]::WriteAllText($profilePath, $profileText, [System.Text.UTF8Encoding]::new($false))
$command = [scriptblock]::Create("& '$literalLauncher' @args")
Set-Item -Path Function:global:termnav -Value $command
Write-Output "Installed the termnav command in $profilePath. It launches TermNav through WSL."
