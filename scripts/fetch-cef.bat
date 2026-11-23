@echo off
REM Downloads and extracts the CEF binary distribution used by NAPP Phase 5.
REM The tarball is ~180 MB and extracts to ~440 MB; it is not committed.
setlocal

set CEF_VERSION=158.0.3+g8c29be2+chromium-158.0.8127.18
set CEF_FILE=cef_binary_%CEF_VERSION%_windows64_minimal.tar.bz2
set CEF_URL=https://cef-builds.spotifycdn.com/cef_binary_158.0.3%%2Bg8c29be2%%2Bchromium-158.0.8127.18_windows64_minimal.tar.bz2

cd /d "%~dp0.."
if exist "third_party\cef\libcef_dll" (
    echo CEF already present in third_party\cef
    exit /b 0
)

if not exist "third_party" mkdir "third_party"
if not exist "build\cef-download" mkdir "build\cef-download"

if not exist "build\cef-download\%CEF_FILE%" (
    echo Downloading %CEF_FILE% ...
    curl -fL --retry 3 -o "build\cef-download\%CEF_FILE%" "%CEF_URL%" || exit /b 1
)

echo Extracting ...
tar -xjf "build\cef-download\%CEF_FILE%" -C third_party || exit /b 1
for /d %%D in (third_party\cef_binary_*) do (
    if exist "third_party\cef" rmdir /s /q "third_party\cef"
    move "%%D" "third_party\cef"
)
echo CEF ready: third_party\cef
exit /b 0
