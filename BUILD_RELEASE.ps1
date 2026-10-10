param()
$ErrorActionPreference='Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root
& "$root\BUILD_RELEASE.bat"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
