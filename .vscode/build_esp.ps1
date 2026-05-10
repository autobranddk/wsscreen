$env:PATH += ';C:\Program Files\Arduino CLI'
arduino-cli compile `
    --fqbn esp32:esp32:esp32s3 `
    --board-options 'PSRAM=opi,FlashSize=16M,FlashMode=qio,PartitionScheme=app3M_fat9M_16MB,USBMode=hwcdc,CDCOnBoot=cdc' `
    'C:\Users\jakob\Documents\wsscreen-1'
if ($LASTEXITCODE -ne 0) {
    Write-Error "ESP build failed"
    exit 1
}
