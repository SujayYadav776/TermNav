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
$termnavEnvironment = @("TERMNAV_IMAGE=$ImageRenderer")
function Get-TermNavUserFolder([string]$WindowsPath, [string]$FolderName) {
    if ($WindowsPath -and (Test-Path -LiteralPath $WindowsPath -PathType Container)) { return $WindowsPath }
    $termnavOneDrive = $env:OneDrive
    if (-not $termnavOneDrive) { $termnavOneDrive = Join-Path ([Environment]::GetFolderPath('UserProfile')) 'OneDrive' }
    $termnavCandidate = Join-Path $termnavOneDrive $FolderName
    if (Test-Path -LiteralPath $termnavCandidate -PathType Container) { return $termnavCandidate }
    return Join-Path ([Environment]::GetFolderPath('UserProfile')) $FolderName
}
$termnavShellFolders = Get-ItemProperty -LiteralPath 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\User Shell Folders' -ErrorAction SilentlyContinue
$termnavDownloads = $termnavShellFolders.'{374DE290-123F-4565-9164-39C4925E467B}'
if ($termnavDownloads) { $termnavDownloads = [Environment]::ExpandEnvironmentVariables($termnavDownloads) }
$termnavUserFolders = @{
    TERMNAV_USER_HOME = [Environment]::GetFolderPath('UserProfile')
    TERMNAV_DOWNLOADS = Get-TermNavUserFolder $termnavDownloads 'Downloads'
    TERMNAV_DOCUMENTS = Get-TermNavUserFolder ([Environment]::GetFolderPath('MyDocuments')) 'Documents'
    TERMNAV_PICTURES = Get-TermNavUserFolder ([Environment]::GetFolderPath('MyPictures')) 'Pictures'
}
foreach ($termnavFolder in $termnavUserFolders.GetEnumerator()) {
    if ($termnavFolder.Value) {
        $termnavLinuxFolder = (& wsl.exe -d $Distribution --exec wslpath -a $termnavFolder.Value).Trim()
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        $termnavEnvironment += "$($termnavFolder.Key)=$termnavLinuxFolder"
    }
}
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
& wsl.exe -d $Distribution --cd $PSScriptRoot --exec env @termnavEnvironment sh -c 'make -s || exit; export TERM=${TERM:-xterm-256color}; exec ./termnav "$@"' termnav @termnavArgs
exit $LASTEXITCODE
