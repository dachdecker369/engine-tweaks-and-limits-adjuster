@echo off
rem SPDX-License-Identifier: GPL-3.0-only
rem Prints a module's PE headers: its DLL characteristics show the exploit mitigations it was built with.
call "%~dp0vsenv.cmd" || exit /b 1
dumpbin /nologo /headers "%~1"
