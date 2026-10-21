@echo off
rem SPDX-License-Identifier: GPL-3.0-only
rem Lists the DLLs a module imports.
call "%~dp0vsenv.cmd" || exit /b 1
dumpbin /nologo /dependents "%~1"
