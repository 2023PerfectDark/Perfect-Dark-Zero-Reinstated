@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 "%~dp0PerfectDarkZeroRecomp\tests\surface_camera_ground_test.cpp" -o "%~dp0surface-camera-test.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0surface-camera-test.exe"
