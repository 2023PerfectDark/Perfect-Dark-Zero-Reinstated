@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 "%~dp0compatibility-v15\PerfectDarkZeroRecomp\tests\debug_position_test.cpp" -o "%~dp0compatibility-v15\debug-position-test.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0compatibility-v15\debug-position-test.exe" save "%~dp0staging\debug-position-test"
if errorlevel 1 exit /b %errorlevel%
"%~dp0compatibility-v15\debug-position-test.exe" restore "%~dp0staging\debug-position-test"
