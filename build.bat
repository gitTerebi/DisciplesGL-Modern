@echo off
setlocal

set "ROOT=%~dp0"
set "MSBUILD=%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"

if not exist "%MSBUILD%" (
  echo MSBuild not found: %MSBUILD%
  exit /b 1
)

"%MSBUILD%" "%ROOT%src\DisciplesGL\DisciplesGL.vcxproj" ^
  /p:Configuration=Release ^
  /p:Platform=Win32 ^
  /p:OutDir="%ROOT%build\\" ^
  /m

if errorlevel 1 exit /b %ERRORLEVEL%

for %%G in ("Disciples 2" "Disciples 2 - Rise of the Elves" "Disciples 2 - Dzieje" "Disciples 2 - Semga") do (
  if not exist "G:\games\%%~G" (
    echo Game folder not found: G:\games\%%~G
    exit /b 1
  )
)

for %%G in ("Disciples 2" "Disciples 2 - Rise of the Elves" "Disciples 2 - Dzieje" "Disciples 2 - Semga") do (
  copy /Y "%ROOT%build\C4dll-R.dll" "G:\games\%%~G\C4dll-R.dll"
  if errorlevel 1 exit /b 1
)

python "%ROOT%tools\d2mt\check.py" "%ROOT%build\C4dll-R.dll"
exit /b 0
