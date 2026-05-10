param(
    [string]$Target = ""   # if set, flash only this target name (e.g. "Screen")
)

$env:PATH += ';C:\Program Files\Arduino CLI'

$configPath = "$PSScriptRoot\esp_targets.json"
$config = Get-Content $configPath -Raw | ConvertFrom-Json

$targets = $config.targets.PSObject.Properties | Where-Object {
    $_.Value.com -and $_.Value.com -ne "" -and $_.Value.sketch -and $_.Value.sketch -ne "" `
    -and ($Target -eq "" -or $_.Name -eq $Target)
}

if (-not $targets) {
    Write-Error "No targets configured with a COM port in esp_targets.json"
    exit 1
}

foreach ($entry in $targets) {
    $name   = $entry.Name
    $t      = $entry.Value
    $com    = $t.com
    $sketch = $t.sketch
    $fqbn   = $t.fqbn
    $opts   = $t.boardOptions

    Write-Host ""
    Write-Host "=== [$name] Compiling $sketch ===" -ForegroundColor Cyan

    $compileArgs = @("compile", "--fqbn", $fqbn)
    if ($opts) { $compileArgs += @("--board-options", $opts) }
    $compileArgs += $sketch

    & arduino-cli @compileArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[$name] Compile failed - aborting"
        exit 1
    }

    Write-Host ""
    Write-Host "=== [$name] Uploading to $com ===" -ForegroundColor Cyan

    $uploadArgs = @("upload", "--fqbn", $fqbn)
    if ($opts) { $uploadArgs += @("--board-options", $opts) }
    $uploadArgs += @("--port", $com, $sketch)

    & arduino-cli @uploadArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[$name] Upload to $com failed"
        exit 1
    }

    Write-Host "[$name] Done." -ForegroundColor Green
}

Write-Host ""
Write-Host "All targets flashed successfully." -ForegroundColor Green
