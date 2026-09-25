@echo off
setlocal

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :novs

for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR goto :novs

set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
call "%VSDIR%\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64 >nul 2>&1
if errorlevel 1 goto :failed

pushd "%~dp0"
if not exist build mkdir build

cl /nologo /std:c++20 /O2 /MT /W4 /EHsc /DWIN32 /DNDEBUG /D_WINDOWS /LD /Fobuild\ /Fdbuild\ ^
  src\dllmain.cpp ^
  src\Hooks\ExeIdentity.cpp ^
  src\Hooks\Hook.cpp ^
  src\Hooks\Log.cpp ^
  src\Hooks\Memory.cpp ^
  src\TrafficAI\AIActionTraffic.cpp ^
  src\TrafficAI\AITraffic.cpp ^
  src\TrafficAI\AITrafficManager.cpp ^
  src\TrafficAI\AIVehicleTraffic.cpp ^
  src\TrafficAI\CollisionWorld.cpp ^
  src\TrafficAI\RandomSurveyor.cpp ^
  src\TrafficAI\TrafficCars.cpp ^
  src\TrafficAI\WRoadNetwork.cpp ^
  /link /DLL /MACHINE:X86 /OUT:ImpTrafficAI.asi /IMPLIB:build\ImpTrafficAI.lib /PDB:build\ImpTrafficAI.pdb kernel32.lib
set "RESULT=%errorlevel%"
popd

if not "%RESULT%"=="0" goto :failed
echo Built ImpTrafficAI.asi
exit /b 0

:novs
echo Visual Studio with the x86 C++ tools was not found.
exit /b 1

:failed
echo Build failed.
exit /b 1
