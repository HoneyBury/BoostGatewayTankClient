@echo off
setlocal
cd /d "%~dp0"

set "ARCHIVE=%~1"
if "%ARCHIVE%"=="" set "ARCHIVE=%cd%\..\third_party-client-assets.zip"
if not exist "%ARCHIVE%" (
    echo ERROR: archive not found: %ARCHIVE%
    exit /b 1
)

powershell -NoProfile -Command "Expand-Archive -Path '%ARCHIVE%' -DestinationPath '%cd%\..' -Force"
echo Restored client third-party assets from %ARCHIVE%
endlocal

