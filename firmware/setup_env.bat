@echo off
echo Initializing Python virtual environment in firmware folder...
python -m venv .venv
if %errorlevel% neq 0 (
    echo Error: Python is not installed or not in PATH!
    pause
    exit /b %errorlevel%
)

echo Installing PlatformIO Core in the virtual environment...
.venv\Scripts\pip.exe install platformio
if %errorlevel% neq 0 (
    echo Error: Failed to install PlatformIO inside virtual environment!
    pause
    exit /b %errorlevel%
)

echo PlatformIO local installation completed successfully!
pause
