@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
set "PATH=%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin;%PATH%"
clang++ -std=c++23 compatibility-v16\PerfectDarkZeroRecomp\tests\input_logic_test.cpp -o compatibility-v16\input_logic_test.exe
if errorlevel 1 exit /b %errorlevel%
compatibility-v16\input_logic_test.exe
if errorlevel 1 exit /b %errorlevel%
clang++ -std=c++23 compatibility-v16\PerfectDarkZeroRecomp\tests\profile_store_test.cpp -o compatibility-v16\profile_store_test.exe
if errorlevel 1 exit /b %errorlevel%
compatibility-v16\profile_store_test.exe
if errorlevel 1 exit /b %errorlevel%
clang++ -std=c++23 -Icompatibility-v16\sdk\include compatibility-v16\PerfectDarkZeroRecomp\tests\virtual_coop_input_test.cpp -o compatibility-v16\virtual_coop_input_test.exe
if errorlevel 1 exit /b %errorlevel%
compatibility-v16\virtual_coop_input_test.exe
