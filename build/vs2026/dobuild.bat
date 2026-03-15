@echo off
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "C:\Code\Quake-2\build\vs2022\game.vcxproj" /p:Configuration=Debug /p:Platform=Win32 /maxcpucount:4 /nologo /verbosity:minimal > "C:\Code\Quake-2\build\vs2022\build_output.txt" 2>&1
echo %ERRORLEVEL% > "C:\Code\Quake-2\build\vs2022\build_exit.txt"
