@echo off
rem SPDX-License-Identifier: GPL-3.0-only
rem Sets up the x64 build environment of the newest Visual Studio with the C++ tools (vswhere).
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" exit /b 1
set "VSDIR="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR exit /b 1
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul
