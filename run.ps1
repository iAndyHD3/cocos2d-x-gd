
Stop-Process -Name "GeometryDash" -ErrorAction SilentlyContinue

$proc = Get-Process -Name "GeometryDash" -ErrorAction SilentlyContinue
if ($proc) {
    $proc.WaitForExit(5000)
}

cmake --build build
# Check if the build command succeeded (Exit Code 0)
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed. Stopping sequence."
    return
}

Copy-Item -Path "build\libcocos2d.dll" -Destination (Split-Path $env:CUSTOM_COCOS_GD) -Force
Start-Process $env:CUSTOM_COCOS_GD; exit