@echo off

cd %~dp0psScripts

:FileCheck
if not exist "%localappdata%/w64cmake" goto InstallCMake
if not exist "%SystemDrive%/vcpkg" goto InstallVcPkg
goto ExitProgram

:InstallCMake
echo Installing CMake...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "InstallCMake.ps1"
echo Waiting for CMake to finish installing...
pause
goto FileCheck

:InstallVcPkg
echo Installing VcPkg...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "InstallVcPkg.ps1"
echo Waiting for VcPkg to finish installing...
pause
goto FileCheck

:ExitProgram
echo Project is already installed...
pause
exit
