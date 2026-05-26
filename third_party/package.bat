@echo off
setlocal
cd /d "%~dp0"
set "PROJECT_DIR=%cd%\.."
set "SOURCE_DIR=%cd%\sdk-package"
set "OUTPUT=%PROJECT_DIR%\third_party-client-assets.zip"

if not exist "%SOURCE_DIR%\lib\cmake\boost_gateway_sdk\boost_gateway_sdk-config.cmake" (
    echo ERROR: third_party\sdk-package is missing.
    echo Run third_party\bootstrap_from_server.bat first.
    exit /b 1
)

if exist "%OUTPUT%" del "%OUTPUT%"
powershell -NoProfile -Command "Compress-Archive -Path '%SOURCE_DIR%' -DestinationPath '%OUTPUT%' -Force"
echo Package created: %OUTPUT%
endlocal

