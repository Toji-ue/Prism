@echo off
rem Prism launcher with logging - double-click me!
cd /d "%~dp0"
echo %date% %time% launching prism.exe >> prism-launch.log
prism.exe 2>> prism-launch.log
echo %date% %time% exited code=%errorlevel% >> prism-launch.log
