@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Bootstrap CMake configuration on Windows using Python script
set ROOT=%~dp0
pushd "%ROOT%"

where python >nul 2>nul
if errorlevel 1 (
    echo Python is required but was not found in PATH.
    echo Please install Python 3 and re-run this script.
    pause
    exit /b 1
)

rem Configure (and build) with default VS 2022 generator
python scripts\setup.py --with-build -c Debug
set ERR=%ERRORLEVEL%
popd

if NOT %ERR%==0 (
    echo CMake configure/build failed with code %ERR%.
    pause
    exit /b %ERR%
)

echo.
echo Done. To build again: cmake --build build --config Debug
pause