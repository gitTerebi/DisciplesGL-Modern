@echo off
setlocal

set "ROOT=%~dp0"
set "MSBUILD=%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"

if not exist "%MSBUILD%" (
  echo MSBuild not found: %MSBUILD%
  exit /b 1
)

if not exist "%ROOT%logs" mkdir "%ROOT%logs"

"%MSBUILD%" "%ROOT%src\DisciplesGL\DisciplesGL.vcxproj" ^
  /p:Configuration=Release ^
  /p:Platform=Win32 ^
  /p:OutDir="%ROOT%build\\" ^
  /m /v:minimal > "%ROOT%logs\build.log" 2>&1
set "BUILDERR=%ERRORLEVEL%"
type "%ROOT%logs\build.log"

if not "%BUILDERR%"=="0" exit /b %BUILDERR%

for %%G in ("Disciples 2" "Disciples 2 - Rise of the Elves" "Disciples 2 - Dzieje" "Disciples 2 - Semga") do (
  if not exist "G:\games\%%~G" (
    echo Game folder not found: G:\games\%%~G
    exit /b 1
  )
)

for %%G in ("Disciples 2" "Disciples 2 - Rise of the Elves" "Disciples 2 - Dzieje" "Disciples 2 - Semga") do (
  if exist "G:\games\%%~G\DisciplesGL-debug.log" move /Y "G:\games\%%~G\DisciplesGL-debug.log" "G:\games\%%~G\DisciplesGL-debug.prev.log" >nul
  if exist "G:\games\%%~G\DisciplesGL-debug.log.old" del /Q "G:\games\%%~G\DisciplesGL-debug.log.old"
  if exist "G:\games\%%~G\DisciplesGL-crash.dmp" del /Q "G:\games\%%~G\DisciplesGL-crash.dmp"
  copy /Y "%ROOT%build\C4dll-R.dll" "G:\games\%%~G\C4dll-R.dll"
  if errorlevel 1 exit /b 1
)

python "%ROOT%tools\d2mt\check.py" "%ROOT%build\C4dll-R.dll"
exit /b 0
