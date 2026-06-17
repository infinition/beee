# Beee Faceplate - Start Companion Backend Server
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# Activate the virtual environment
$venvPath = Join-Path $scriptDir "..\.venv\Scripts\Activate.ps1"
if (Test-Path $venvPath) {
    Write-Host "[Beee] Activating virtual environment..." -ForegroundColor Cyan
    . $venvPath
} else {
    Write-Host "[Beee] Warning: Virtual environment at $venvPath not found. Running with global python." -ForegroundColor Yellow
}

# Navigate to python server directory
$serverDir = Join-Path $scriptDir "python_server"
Set-Location $serverDir

Write-Host "[Beee] Starting FastAPI Backend on http://localhost:8000 ..." -ForegroundColor Green
python server.py
