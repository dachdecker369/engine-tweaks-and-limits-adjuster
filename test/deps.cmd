@echo off
rem Lists the DLLs a module imports.
call "%~dp0vsenv.cmd" || exit /b 1
dumpbin /nologo /dependents "%~1"
