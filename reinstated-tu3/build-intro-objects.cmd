@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
set "PATH=%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin;C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
cd /d "%~dp0PerfectDarkZeroRecomp"
cmake --preset win-amd64-release
if errorlevel 1 exit /b %errorlevel%
cmake --build --preset win-amd64-release --parallel 8 --target CMakeFiles/perfectdarkzerorecomp.dir/src/surface_runtime.cpp.obj CMakeFiles/perfectdarkzerorecomp.dir/src/visible_holsters.cpp.obj

