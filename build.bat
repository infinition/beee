@echo off
REM Beee build and flash launcher.
REM Asks for COM port, language and WiFi on first run, then compiles and uploads.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1"
echo.
pause
