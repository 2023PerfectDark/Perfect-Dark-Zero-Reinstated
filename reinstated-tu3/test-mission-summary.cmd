@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 "%~dp0compatibility-v15\PerfectDarkZeroRecomp\tests\mission_summary_test.cpp" -o "%~dp0compatibility-v15\mission-summary-test.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0compatibility-v15\mission-summary-test.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 "%~dp0compatibility-v15\PerfectDarkZeroRecomp\tests\cheat_progress_test.cpp" -o "%~dp0compatibility-v15\cheat-progress-test.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0compatibility-v15\cheat-progress-test.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 "%~dp0compatibility-v15\PerfectDarkZeroRecomp\tests\npc_placement_test.cpp" -o "%~dp0compatibility-v15\npc-placement-test.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0compatibility-v15\npc-placement-test.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 "%~dp0compatibility-v15\PerfectDarkZeroRecomp\tests\guard_wave_test.cpp" -o "%~dp0compatibility-v15\guard-wave-test.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0compatibility-v15\guard-wave-test.exe"
