$env:PATH = 'C:\msys64\ucrt64\bin;' + $env:PATH
Set-Location 'C:\Users\jakob\Documents\lv_sim\build'
ninja
if ($LASTEXITCODE -ne 0) {
    Write-Error "Simulator build failed"
    exit 1
}
