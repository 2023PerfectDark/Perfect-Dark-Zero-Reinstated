@echo off
setlocal
if not defined PDZ_SETUP_OUTPUT set "PDZ_SETUP_OUTPUT=installer\PDZ Setup.exe"
set "PDZ_CSC=C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
set "PDZ_WPF=C:\Windows\Microsoft.NET\Framework64\v4.0.30319\WPF"
"%PDZ_CSC%" /nologo /target:winexe /platform:x64 /optimize+ /out:"installer\PDZ Bonus DVD.exe" /r:System.Drawing.dll /r:System.Windows.Forms.dll installer\Bonus.cs
if errorlevel 1 exit /b 1
"%PDZ_CSC%" /nologo /target:winexe /platform:x64 /optimize+ /out:"%PDZ_SETUP_OUTPUT%" /r:System.Xaml.dll /r:System.Web.Extensions.dll /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll /r:System.Windows.Forms.dll /r:"%PDZ_WPF%\WindowsBase.dll" /r:"%PDZ_WPF%\PresentationCore.dll" /r:"%PDZ_WPF%\PresentationFramework.dll" installer\Core.cs installer\Setup.cs installer\Dpal.cs installer\Welcome.cs installer\Comics.cs
