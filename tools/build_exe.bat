@echo off
REM Build beee_agent.exe (single file, no console window).
REM The git + active-window features use only the standard library.
REM Install psutil first if you want the optional CPU-load reaction.
cd /d "%~dp0"
pip install pyinstaller
pyinstaller --onefile --noconsole --name beee_agent beee_agent.py
echo.
echo Done. The executable is in tools\dist\beee_agent.exe
echo Put beee_agent_config.json next to the exe, then run it (or add a shortcut
echo to it in shell:startup to launch Beee's agent at login).
pause
