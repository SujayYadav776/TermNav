<# Launch TermNav from PowerShell using WSL. #>
param(
    [string]$Path = '.',
    [string]$Distribution = 'TermNav-Dev',
    [switch]$All,
    [switch]$Ascii,
    [switch]$NoColor,
    [ValidateSet('auto', 'sixel', 'blocks')]
    [string]$ImageRenderer = 'auto',
    [switch]$Demo
)
$ErrorActionPreference = 'Stop'
if (-not (Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
    throw 'WSL is required. See README.md for Linux/WSL setup.'
}
$termnavArgs = @()
if ($All) { $termnavArgs += '--all' }
if ($Ascii) { $termnavArgs += '--ascii' }
if ($NoColor) { $termnavArgs += '--no-color' }
if ($Demo) {
    & wsl.exe -d $Distribution --cd $PSScriptRoot --exec python3 scripts/create_demo.py
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $Path = 'test-playground'
    $termnavArgs += '--home'
} elseif ($Path -match '^[A-Za-z]:[\\/]') {
    $Path = (& wsl.exe -d $Distribution --exec wslpath -a $Path).Trim()
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
if ($Demo -or $PSBoundParameters.ContainsKey('Path')) {
    $termnavArgs += '--'
    $termnavArgs += $Path
}
& wsl.exe -d $Distribution --cd $PSScriptRoot --exec env "TERMNAV_IMAGE=$ImageRenderer" sh -c 'make -s || exit; export TERM=${TERM:-xterm-256color}; exec ./termnav "$@"' termnav @termnavArgs
exit $LASTEXITCODE
