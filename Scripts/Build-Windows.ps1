[CmdletBinding()]
param(
    [string]$UnrealRoot = $env:UE_ROOT,
    [string]$OutputRoot = $env:OUTPUT_ROOT
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ProjectFile = Join-Path $ProjectRoot "AshesOfHeaven.uproject"
if ([string]::IsNullOrWhiteSpace($UnrealRoot)) {
    $UnrealRoot = "C:\Program Files\Epic Games\UE_5.8"
}
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $ProjectRoot "Builds\Windows"
}
$Uat = Join-Path $UnrealRoot "Engine\Build\BatchFiles\RunUAT.bat"

if ($env:OS -ne "Windows_NT") {
    throw "Windows packaging must run on a Windows x64 build machine."
}
if (-not (Test-Path $Uat)) {
    throw "RunUAT.bat not found at $Uat. Set UE_ROOT to the installed Unreal Engine root."
}

New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
Write-Host "Building AshesOfHeaven Windows x64 Shipping package..."
& $Uat BuildCookRun `
    "-project=$ProjectFile" -noP4 -platform=Win64 -clientconfig=Shipping `
    -build -cook -stage -pak -archive -prereqs "-archivedirectory=$OutputRoot"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Windows output: $OutputRoot"
Get-ChildItem -Path $OutputRoot -Filter "AshesOfHeaven.exe" -Recurse | Select-Object -ExpandProperty FullName
