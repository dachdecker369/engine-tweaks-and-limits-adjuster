@echo off
rem junction.cmd LINK TARGET - makes the folder LINK a junction to TARGET (no administrator needed).
rem junction.cmd remove LINK - removes the junction LINK itself, never what is in its target.
rem The word is "remove", not "/remove": Git Bash turns an argument that starts with / into a path.
if /i "%~1"=="remove" (rmdir "%~2") else (mklink /J "%~1" "%~2" >nul)
