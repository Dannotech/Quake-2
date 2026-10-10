@echo off
setlocal
set "CONFIG=%~1"
set "PLATFORM=%~2"
if not defined CONFIG set "CONFIG=Debug"
if not defined PLATFORM set "PLATFORM=x64"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo Visual Studio Installer vswhere.exe was not found.
    exit /b 1
)
for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%I"
if not defined MSBUILD (
    echo Install Visual Studio with Desktop development with C++.
    exit /b 1
)
"%MSBUILD%" "%~dp0quake2.sln" /p:Configuration=%CONFIG% /p:Platform=%PLATFORM% /m /nologo /v:minimal
exit /b %ERRORLEVEL%
