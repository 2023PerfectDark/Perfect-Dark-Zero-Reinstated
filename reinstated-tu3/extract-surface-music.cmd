@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0PerfectDarkZeroRecomp"
cl /nologo /EHsc /std:c++17 tools\surface\extract-music.cpp /Foout\surface\extract-music.obj /Feout\surface\extract-music.exe /link mfplat.lib mfreadwrite.lib mfuuid.lib ole32.lib
if errorlevel 1 exit /b %errorlevel%
out\surface\extract-music.exe "C:\Users\Owner\Downloads\Video\Goldeneye- Surface 1 Theme Song Extended.mp4" out\surface\mission-music.wav
