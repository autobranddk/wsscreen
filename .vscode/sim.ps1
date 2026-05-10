Stop-Process -Name 'main' -ErrorAction SilentlyContinue
& "$PSScriptRoot\build_sim.ps1"
if ($LASTEXITCODE -ne 0) { exit 1 }
Start-Process 'C:\Users\jakob\Documents\lv_sim\bin\main.exe'
