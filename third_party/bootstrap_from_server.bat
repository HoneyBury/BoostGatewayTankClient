@echo off
setlocal
cd /d "%~dp0"

set "SERVER_ROOT=%~1"
if "%SERVER_ROOT%"=="" set "SERVER_ROOT=%cd%\..\..\BoostAsioDemo"
set "SDK_PREFIX=%SERVER_ROOT%\runtime\sdk-package-prefix"
set "DEST=%cd%\sdk-package"

if not exist "%SDK_PREFIX%\lib\cmake\boost_gateway_sdk\boost_gateway_sdk-config.cmake" (
    echo ERROR: SDK package config not found under %SDK_PREFIX%
    echo Build and install the server SDK first.
    exit /b 1
)

if exist "%DEST%" (
    echo [skip] third_party\sdk-package already exists
    exit /b 0
)

echo [copy] %SDK_PREFIX% -^> %DEST%
robocopy "%SDK_PREFIX%" "%DEST%" /E >nul
if errorlevel 8 (
    echo ERROR: robocopy failed while copying SDK package
    exit /b 1
)

echo Done. Client third_party SDK package is ready.
endlocal

