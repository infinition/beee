@echo off
REM Build beee_agent.exe (single file, no console window).
REM The git + active-window features use only the standard library.
REM Install psutil first if you want the optional CPU-load reaction.
cd /d "%~dp0"
pip install pyinstaller
pyinstaller --onefile --noconsole --name beee_agent beee_agent.py

REM The exe reads beee_agent_config.json from its own folder, so copy it next to it.
if exist beee_agent_config.json copy /Y beee_agent_config.json dist\beee_agent_config.json >nul

echo.
echo Done. The executable is in tools\dist\beee_agent.exe
echo Keep beee_agent_config.json next to the exe (already copied if it existed).
echo Add a shortcut to the exe in shell:startup to launch it at login.
echo A beee_agent.log file is written next to the exe for debugging.
pause
