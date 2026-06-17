# Beee Faceplate - Flash ESP32-S3 Firmware
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# Activate the virtual environment to access pio
$venvPath = Join-Path $scriptDir "..\.venv\Scripts\Activate.ps1"
if (Test-Path $venvPath) {
    . $venvPath
}

# Navigate to firmware directory
$firmwareDir = Join-Path $scriptDir "esp32_firmware"
Set-Location $firmwareDir

Write-Host "[Beee] Compiling and uploading firmware to COM3..." -ForegroundColor Cyan
pio run --target upload --upload-port COM3

if ($LASTEXITCODE -eq 0) {
    Write-Host "[Beee] Success! Firmware compiled and uploaded to COM3." -ForegroundColor Green
    Write-Host "[Beee] Opening Serial Monitor (Press Ctrl+C to exit)..." -ForegroundColor Cyan
    pio device monitor -b 115200
} else {
    Write-Host "[Beee] Error: Failed to compile or upload firmware." -ForegroundColor Red
}
