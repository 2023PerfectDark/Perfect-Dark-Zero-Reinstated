@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 "%~dp0compatibility-v15\PerfectDarkZeroRecomp\tests\checkpoint_continuation_test.cpp" -o "%~dp0compatibility-v15\checkpoint-continuation-test.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0compatibility-v15\checkpoint-continuation-test.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0compatibility-v15\checkpoint-continuation-test.exe" capture "%~dp0staging\checkpoint-continuation-test"
if errorlevel 1 exit /b %errorlevel%
"%~dp0compatibility-v15\checkpoint-continuation-test.exe" restore "%~dp0staging\checkpoint-continuation-test"
exit /b %errorlevel%
