@echo off

if not exist "%localappdata%/w64cmake/bin" goto BuildError
if not exist "%SystemDrive%/vcpkg" goto BuildError

set PATH=%localappdata%/w64cmake/bin;%SystemDrive%/vcpkg;%PATH%

cd %~dp0..


set env=%1

:BuildProject
cmake -S %cd% -B %cd%\out -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="%SystemDrive%/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build %cd%\out --config Release
if %ERRORLEVEL% equ 0 goto BuildSucceeded

:BuildError
echo Build failed!
if /i "%env%"=="VSCODE" goto BuildExit
pause
goto BuildExit

:BuildSucceeded
echo Build succeeded. Launching game...
for %%i in ("%cd%\out\Release\*.exe") do start "" "%%i"
goto BuildExit

:BuildExit
echo Closing Builder...
exit
