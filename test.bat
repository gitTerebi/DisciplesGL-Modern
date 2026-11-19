@echo off
setlocal

set "ROOT=%~dp0"
set "LOGS=%ROOT%logs"
pushd "%ROOT%"
for /f "delims=" %%B in ('git rev-parse --abbrev-ref HEAD') do set "BRANCH=%%B"
if /i "%~1"=="send" goto send

echo === sync %BRANCH%
git pull --rebase --autostash origin %BRANCH%
if errorlevel 1 (
  echo git pull failed, fix it and run again
  pause
  exit /b 1
)

echo === build
call "%ROOT%build.bat"
if errorlevel 1 (
  echo === build failed, sending the build log
  goto send
)

echo.
echo === build done. Play the game now: menus, the map, a battle.
echo === close the game, then press any key here to send the logs.
pause >nul

:send
if not exist "%LOGS%" mkdir "%LOGS%"
del /Q "%LOGS%\*-*.log" "%LOGS%\*.old" "%LOGS%\*.png" "%LOGS%\*.dmp" "%LOGS%\*.map" "%LOGS%\*.bin" 2>nul
if exist "%ROOT%build\C4dll-R.map" copy /Y "%ROOT%build\C4dll-R.map" "%LOGS%\C4dll-R.map" >nul
call :grab "Disciples 2" d2
call :grab "Disciples 2 - Rise of the Elves" rote
call :grab "Disciples 2 - Dzieje" dzieje
call :grab "Disciples 2 - Semga" semga

echo === send logs
git add -A -f logs
git commit -m "logs: test run %DATE% %TIME%"
git pull --rebase --autostash origin %BRANCH%
git push origin %BRANCH%
popd
echo === done
pause
exit /b 0

:grab
set "GAME=G:\games\%~1"
for %%F in (DisciplesGL-debug.log DisciplesGL-debug.prev.log) do (
  if exist "%GAME%\%%F" (
    echo found %GAME%\%%F
    powershell -NoProfile -Command "Get-Content -LiteralPath '%GAME%\%%F' -Tail 60000 | Set-Content -LiteralPath '%LOGS%\%~2-%%F'"
  )
)
rem the DLL rotates DisciplesGL-debug.log to DisciplesGL-debug.log.old at 8 MB; the start of the run is in there
if exist "%GAME%\DisciplesGL-debug.log.old" (
  echo found %GAME%\DisciplesGL-debug.log.old
  powershell -NoProfile -Command "Get-Content -LiteralPath '%GAME%\DisciplesGL-debug.log.old' -Tail 60000 | Set-Content -LiteralPath '%LOGS%\%~2-DisciplesGL-debug.old.log'"
)
rem twin-code.bin holds the game code around the addresses the log names, for disassembly in the next round
for %%P in (twin-screen.png twin-sheet.png twin-1x.png twin-code.bin DisciplesGL-crash.dmp) do if exist "%GAME%\%%P" copy /Y "%GAME%\%%P" "%LOGS%\%~2-%%P" >nul
if exist "%GAME%\imagecoverage.txt" copy /Y "%GAME%\imagecoverage.txt" "%LOGS%\%~2-imagecoverage.txt" >nul
exit /b 0
