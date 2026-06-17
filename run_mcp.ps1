# Beee Faceplate - Start MCP Server
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# Activate the virtual environment
$venvPath = Join-Path $scriptDir "..\.venv\Scripts\Activate.ps1"
if (Test-Path $venvPath) {
    Write-Host "[Beee] Activating virtual environment..." -ForegroundColor Cyan
    . $venvPath
} else {
    Write-Host "[Beee] Warning: Virtual environment at $venvPath not found." -ForegroundColor Yellow
}

# Navigate to mcp server directory
$mcpDir = Join-Path $scriptDir "mcp_server"
Set-Location $mcpDir

Write-Host "[Beee] Starting MCP Server in stdio mode..." -ForegroundColor Green
python mcp_server.py
