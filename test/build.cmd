@echo off
rem SPDX-License-Identifier: GPL-3.0-only
rem Builds the test programs into %OUT_DIR%: the plain fake game and the full fake game, the
rem latter once per game build it pretends to be (version resource) and once without a version, all with
rem warnings as errors (/WX).
call "%~dp0vsenv.cmd" || exit /b 1
set "SRC_DIR=%~dp0"
cd /d "%OUT_DIR%" || exit /b 2
rc /nologo /dVER_C=1158 /dVER_D=16 /fo v1158_16.res "%SRC_DIR%fake_game_version.rc" || exit /b 5
rc /nologo /dVER_C=1158 /dVER_D=13 /fo v1158_13.res "%SRC_DIR%fake_game_version.rc" || exit /b 5
rc /nologo /dVER_C=1013 /dVER_D=17 /fo v1013_17.res "%SRC_DIR%fake_game_version.rc" || exit /b 5
rc /nologo /dVER_C=889 /dVER_D=22 /fo v889_22.res "%SRC_DIR%fake_game_version.rc" || exit /b 5
cl /nologo /W4 /WX /EHsc /DUNICODE /D_UNICODE "%SRC_DIR%fake_game.cpp" v1158_16.res /Fe:plain_game.exe /Fo:fake_game.obj /link /WX || exit /b 3
cl /nologo /W4 /WX /EHsc /std:c++17 /DUNICODE /D_UNICODE /c "%SRC_DIR%fake_game_full.cpp" /Fo:fake_game_full.obj || exit /b 4
cl /nologo /W4 /WX /EHsc /std:c++17 /DUNICODE /D_UNICODE /c "%SRC_DIR%fake_game_rov.cpp" /Fo:fake_game_rov.obj || exit /b 4
link /nologo /WX fake_game_full.obj fake_game_rov.obj /SECTION:.fakesit,ERW v1158_16.res /out:full_game.exe || exit /b 6
link /nologo /WX fake_game_full.obj fake_game_rov.obj /SECTION:.fakesit,ERW v1158_13.res /out:full_game_1158_13.exe || exit /b 6
link /nologo /WX fake_game_full.obj fake_game_rov.obj /SECTION:.fakesit,ERW v1013_17.res /out:full_game_1013.exe || exit /b 6
link /nologo /WX fake_game_full.obj fake_game_rov.obj /SECTION:.fakesit,ERW v889_22.res /out:full_game_889.exe || exit /b 6
link /nologo /WX fake_game_full.obj fake_game_rov.obj /SECTION:.fakesit,ERW /out:full_game_nover.exe || exit /b 6
rem the stand-in dstorage.dll (option "dstorage")
cl /nologo /W4 /WX /LD "%SRC_DIR%fake_dstorage.cpp" /Fe:dstorage.dll /Fo:fake_dstorage.obj /link /WX || exit /b 7
echo == BUILT
