[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$ScriptPath,

    [ValidatePattern("^[A-Z]$")]
    [string]$DriveLetter = "U"
)

$ErrorActionPreference = "Stop"

$workspace = Resolve-Path "$PSScriptRoot\.."
$driveName = "$DriveLetter`:"
$projectFile = "$driveName\GenesisDigitalTwin\GenesisDigitalTwin.uproject"
$editorExe = "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe"
$resolvedScript = Resolve-Path $ScriptPath

if (-not (Test-Path -LiteralPath $editorExe)) {
    throw "UnrealEditor.exe was not found: $editorExe"
}

$existingMapping = (& subst.exe) | Where-Object {
    $_ -match "^$([regex]::Escape($driveName))\\:"
}

if ($existingMapping) {
    throw "Drive $driveName is already in use. Pass another letter, for example: -DriveLetter V"
}

& subst.exe $driveName $workspace
if ($LASTEXITCODE -ne 0) {
    throw "Failed to map $driveName to $workspace"
}

try {
    & $editorExe `
        $projectFile `
        "/Game/Maps/MainMaps" `
        -ExecutePythonScript="$resolvedScript" `
        -unattended `
        -nop4 `
        -nosplash `
        -NoSound

    if ($LASTEXITCODE -ne 0) {
        throw "UnrealEditor failed with exit code $LASTEXITCODE"
    }
}
finally {
    & subst.exe $driveName /D
}
