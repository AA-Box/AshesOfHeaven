[CmdletBinding()]
param(
    [string]$UnrealRoot = $env:UE_ROOT,
    [string]$OutputRoot = $env:OUTPUT_ROOT,
    [string]$ClientConfig = $env:CLIENT_CONFIG
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ProjectFile = Join-Path $ProjectRoot "AshesOfHeaven.uproject"
$PsoValidator = Join-Path $PSScriptRoot "Validate-PSO.py"
if ([string]::IsNullOrWhiteSpace($UnrealRoot)) {
    $UnrealRoot = "C:\Program Files\Epic Games\UE_5.8"
}
if ([string]::IsNullOrWhiteSpace($ClientConfig)) {
    $ClientConfig = "Shipping"
}
if ($ClientConfig -notin @("Development", "Shipping")) {
    throw "CLIENT_CONFIG must be Development or Shipping."
}
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = if ($ClientConfig -eq "Shipping") {
        Join-Path $ProjectRoot "Builds\Windows"
    } else {
        Join-Path $ProjectRoot "Builds\Windows-$ClientConfig"
    }
}
$Uat = Join-Path $UnrealRoot "Engine\Build\BatchFiles\RunUAT.bat"

if ($env:OS -ne "Windows_NT") {
    throw "Windows packaging must run on a Windows x64 build machine."
}
if (-not (Test-Path $Uat)) {
    throw "RunUAT.bat not found at $Uat. Set UE_ROOT to the installed Unreal Engine root."
}

function Invoke-PsoValidator {
    param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Arguments)
    if (Get-Command py -ErrorAction SilentlyContinue) {
        & py -3 $PsoValidator @Arguments
    } elseif (Get-Command python3 -ErrorAction SilentlyContinue) {
        & python3 $PsoValidator @Arguments
    } elseif (Get-Command python -ErrorAction SilentlyContinue) {
        & python $PsoValidator @Arguments
    } else {
        throw "Python 3 is required for Scripts/Validate-PSO.py."
    }
    if ($LASTEXITCODE -ne 0) { throw "PSO validation failed with exit code $LASTEXITCODE." }
}

Invoke-PsoValidator config --platform windows

New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
Write-Host "Building AshesOfHeaven Windows x64 $ClientConfig package..."
& $Uat BuildCookRun `
    "-project=$ProjectFile" -noP4 -platform=Win64 "-clientconfig=$ClientConfig" `
    -build -cook -stage -pak -archive -prereqs "-archivedirectory=$OutputRoot"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$PackageValidationArgs = @(
    "package", "--platform", "windows",
    "--staged-root", (Join-Path $ProjectRoot "Saved\StagedBuilds\Windows"),
    "--archive-root", $OutputRoot
)
if ($env:AH_REQUIRE_PSO_CACHE -eq "1") {
    $PackageValidationArgs += "--require-bundled-cache"
}
Invoke-PsoValidator @PackageValidationArgs

Write-Host "Windows output: $OutputRoot"
Get-ChildItem -Path $OutputRoot -Filter "AshesOfHeaven.exe" -Recurse | Select-Object -ExpandProperty FullName
