@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 "%~dp0PerfectDarkZeroRecomp\tests\input_logic_test.cpp" -o "%~dp0input-logic-test.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0input-logic-test.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 "%~dp0PerfectDarkZeroRecomp\tests\campaign_logic_test.cpp" -o "%~dp0campaign-logic-test.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0campaign-logic-test.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 "%~dp0PerfectDarkZeroRecomp\tests\physics_timing_test.cpp" -o "%~dp0physics-timing-test.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0physics-timing-test.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 "%~dp0PerfectDarkZeroRecomp\tests\holster_logic_test.cpp" -o "%~dp0holster-logic-test.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0holster-logic-test.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 "%~dp0PerfectDarkZeroRecomp\tests\profile_store_test.cpp" -o "%~dp0profile-store-test.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0profile-store-test.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 "%~dp0PerfectDarkZeroRecomp\tests\surface_mission_test.cpp" -o "%~dp0surface-mission-test.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0surface-mission-test.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 "%~dp0PerfectDarkZeroRecomp\tests\coop_ally_logic_test.cpp" -o "%~dp0coop-ally-logic-test.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0coop-ally-logic-test.exe"
