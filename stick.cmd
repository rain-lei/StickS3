@echo off
setlocal
cd /d "%~dp0" || exit /b 1
set "PLATFORMIO_CORE_DIR=%CD%\.platformio"
set "PIP_CACHE_DIR=%CD%\.cache\pip"
set "TEMP=%CD%\.tmp"
set "TMP=%TEMP%"
set "PIP_DISABLE_PIP_VERSION_CHECK=1"
set "VIRTUAL_ENV=%CD%\.venv"
set "PATH=%VIRTUAL_ENV%\Scripts;%PATH%"
if not exist "%TEMP%" mkdir "%TEMP%" || exit /b 1
if /i "%~1"=="setup" goto setup
if not exist ".venv\Scripts\pio.exe" (
    echo Run stick.cmd setup first.
    exit /b 1
)
if "%~1"=="" goto build
".venv\Scripts\pio.exe" %*
exit /b %errorlevel%

:setup
if not exist ".venv\Scripts\python.exe" (
    py -3.11 -m venv .venv || exit /b 1
)
".venv\Scripts\python.exe" -m pip install --retries 2 platformio || exit /b 1
:build
".venv\Scripts\pio.exe" run
exit /b %errorlevel%
