@echo off
REM Double-click entry point for the NAPP setup.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0NAPP-Setup.ps1" %*
if errorlevel 1 pause
