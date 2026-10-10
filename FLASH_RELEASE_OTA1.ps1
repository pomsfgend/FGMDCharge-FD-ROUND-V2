param([Parameter(Mandatory=$true)][string]$Port)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root
$bat = Join-Path $root 'FLASH_RELEASE_OTA1.bat'
if (!(Test-Path $bat)) { throw 'FLASH_RELEASE_OTA1.bat is missing.' }
cmd.exe /c "set CUBE_PORT=$Port&& call `"$bat`""
exit $LASTEXITCODE
