@echo off
REM NAPP Windows build: configure + build + optional test run.
REM Usage: scripts\build.bat [Debug|Release] [--test]
setlocal

cd /d "%~dp0.."
set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Debug

set PATH=C:\Qt\Tools\QtCreator\bin;%PATH%
REM vcvars64 may change the current directory; keep and restore ours.
set "VSCMD_START_DIR=%CD%"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo [ERROR] vcvars64.bat not found
    exit /b 1
)
cd /d "%~dp0.."

cmake -S . -B build\dev -G Ninja -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/msvc2022_64
if errorlevel 1 exit /b 2

cmake --build build\dev --parallel
if errorlevel 1 exit /b 3

if "%2"=="--test" (
    ctest --test-dir build\dev --output-on-failure
    if errorlevel 1 exit /b 4
)

exit /b 0
