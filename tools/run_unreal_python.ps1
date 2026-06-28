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
$editorCmd = "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$resolvedScript = Resolve-Path $ScriptPath

if (-not (Test-Path -LiteralPath $editorCmd)) {
    throw "UnrealEditor-Cmd.exe was not found: $editorCmd"
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
    & $editorCmd `
        $projectFile `
        -run=pythonscript `
        -script="$resolvedScript" `
        -unattended `
        -nop4 `
        -nosplash `
        -NoSound

    if ($LASTEXITCODE -ne 0) {
        throw "UnrealEditor-Cmd failed with exit code $LASTEXITCODE"
    }
}
finally {
    & subst.exe $driveName /D
}
