[CmdletBinding()]
param(
    [ValidateSet("Debug", "DebugGame", "Development", "Shipping", "Test")]
    [string]$Configuration = "Development",

    [ValidatePattern("^[A-Z]$")]
    [string]$DriveLetter = "U"
)

$ErrorActionPreference = "Stop"

$workspace = $PSScriptRoot
$projectFile = "$DriveLetter`:\GenesisDigitalTwin\GenesisDigitalTwin.uproject"
$buildScript = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat"
$driveName = "$DriveLetter`:"

if (-not (Test-Path -LiteralPath $buildScript)) {
    throw "Unreal Engine 5.7 build script was not found: $buildScript"
}

$existingMapping = (& subst.exe) | Where-Object {
    $_ -match "^$([regex]::Escape($driveName))\\:"
}

if ($existingMapping) {
    throw "Drive $driveName is already in use. Pass another letter, for example: .\BuildEditor.ps1 -DriveLetter V"
}

& subst.exe $driveName $workspace
if ($LASTEXITCODE -ne 0) {
    throw "Failed to map $driveName to $workspace"
}

try {
    & $buildScript `
        GenesisDigitalTwinEditor `
        Win64 `
        $Configuration `
        $projectFile `
        -WaitMutex `
        -NoHotReloadFromIDE `
        -NoUBA

    if ($LASTEXITCODE -ne 0) {
        throw "UnrealBuildTool failed with exit code $LASTEXITCODE"
    }
}
finally {
    & subst.exe $driveName /D
}
