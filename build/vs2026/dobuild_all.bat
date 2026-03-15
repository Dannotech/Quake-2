@echo off
set MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe
set OUT=C:\Code\Quake-2\build\vs2022\build_all_output.txt

echo Building all projects (Debug|Win32) > "%OUT%"
echo ====================================== >> "%OUT%"

for %%P in (game.vcxproj ref_gl.vcxproj ctf.vcxproj ref_soft.vcxproj quake2.vcxproj) do (
    echo. >> "%OUT%"
    echo === %%P === >> "%OUT%"
    "%MSBUILD%" "C:\Code\Quake-2\build\vs2022\%%P" /p:Configuration=Debug /p:Platform=Win32 /nologo /verbosity:minimal >> "%OUT%" 2>&1
    echo Exit: %ERRORLEVEL% >> "%OUT%"
)

echo DONE >> "%OUT%"
echo %ERRORLEVEL% > "C:\Code\Quake-2\build\vs2022\build_all_exit.txt"
