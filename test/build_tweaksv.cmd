@echo off
rem SPDX-License-Identifier: GPL-3.0-only
rem Builds TweaksV.asi for the tests: Release (x64\Release) and the test build (object\Test\Release).
call "%~dp0vsenv.cmd" || exit /b 1
msbuild "%~dp0..\source\TweaksV.slnx" -nologo -v:minimal -p:Configuration=Release -p:Platform=x64 || exit /b 2
msbuild "%~dp0..\source\TweaksV.slnx" -nologo -v:minimal -p:Configuration=Release -p:Platform=x64 -p:TweaksVTestBuild=true || exit /b 3
