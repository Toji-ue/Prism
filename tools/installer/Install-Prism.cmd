@echo off
rem Double-click installer wrapper. Bypasses the execution policy so the
rem tester does not have to fight PowerShell before the app is even installed.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Prism-Setup.ps1"
