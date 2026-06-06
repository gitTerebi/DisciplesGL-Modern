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

set "GAME_DIR=G:\games\Disciples 2"
if not exist "%GAME_DIR%" (
  echo Game folder not found: %GAME_DIR%
  exit /b 1
)

copy /Y "%ROOT%build\C4dll-R.dll" "%GAME_DIR%\C4dll-R.dll"
exit /b %ERRORLEVEL%
