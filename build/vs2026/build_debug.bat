@echo off
setlocal

set MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe

echo ============================================================
echo Building Quake 2 VS2022 solution (Debug^|Win32)
echo ============================================================

"%MSBUILD%" "%~dp0quake2.sln" /p:Configuration=Debug /p:Platform=Win32 /maxcpucount:4 /nologo /verbosity:minimal 2>&1

echo.
echo Build complete. Exit code: %ERRORLEVEL%
endlocal
