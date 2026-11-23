@echo off
REM NAPP Phase 5 build: Release + Chromium (CEF) runtime host.
REM Usage: scripts\build-cef.bat [--test] [--dist]
REM   --dist  after building, run scripts\make-dist.ps1 to produce
REM           dist\NAPP-<version>-Setup.exe and dist\NAPP-<version>-portable.zip
setlocal

REM Capture the project root before the argument loop: `shift` also moves %0,
REM so %~dp0 no longer points at this script afterwards.
set "ROOT=%~dp0.."
cd /d "%ROOT%"

set RUN_TESTS=
set MAKE_DIST=
:parse
if "%~1"=="" goto parsed
if /i "%~1"=="--test" set RUN_TESTS=1
if /i "%~1"=="--dist" set MAKE_DIST=1
shift
goto parse
:parsed

set PATH=C:\Qt\Tools\QtCreator\bin;%PATH%
REM vcvars64 may change the current directory; keep and restore ours.
set "VSCMD_START_DIR=%CD%"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
cd /d "%ROOT%"

cmake -S . -B build\cef -G Ninja -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/msvc2022_64 ^
      -DNAPP_ENABLE_CEF=ON
if errorlevel 1 exit /b 2

cmake --build build\cef --parallel
if errorlevel 1 exit /b 3

if defined RUN_TESTS (
    ctest --test-dir build\cef --output-on-failure
    if errorlevel 1 exit /b 4
)

if defined MAKE_DIST (
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File "scripts\make-dist.ps1" -SkipBuild
    if errorlevel 1 exit /b 5
)

exit /b 0
