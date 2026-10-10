@echo off
call "%~dp0build.bat" Debug x64
exit /b %ERRORLEVEL%
