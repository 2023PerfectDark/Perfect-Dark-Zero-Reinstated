@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /EHsc notification-timeout-test.cpp /Fe:notification-timeout-test.exe /Fo:notification-timeout-test.obj > notification-timeout-test.log 2>&1
if errorlevel 1 exit /b 1
notification-timeout-test.exe
