@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0"
"C:\Users\Owner\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe" tools\build-stock-tu3.py
