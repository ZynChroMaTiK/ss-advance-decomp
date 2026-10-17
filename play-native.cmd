@echo off
setlocal
pushd "%~dp0" || exit /b 1
if not exist "build\logs" mkdir "build\logs"
if defined SSA_LAUNCH_LOG (
    set "SSA_RUN_LOG=%SSA_LAUNCH_LOG%"
    goto run_game
)
set /a SSA_LOG_TRIES=0 >nul
:choose_log
set /a SSA_LOG_TRIES+=1 >nul
if %SSA_LOG_TRIES% GTR 32 (
    echo Could not create a session log under build\logs.
    popd
    exit /b 1
)
set "SSA_SESSION_DIR=build\logs\launch-%RANDOM%-%RANDOM%-%RANDOM%"
mkdir "%SSA_SESSION_DIR%" 2>nul
if errorlevel 1 goto choose_log
set "SSA_RUN_LOG=%SSA_SESSION_DIR%\stderr.log"
:run_game
"build\native-windows\Release\ssa_native_game.exe" "orig\SSA.gba" %* 2>"%SSA_RUN_LOG%"
set "SSA_LAUNCH_EXIT=%ERRORLEVEL%"
if not defined SSA_LAUNCH_LOG copy /y "%SSA_RUN_LOG%" "build\logs\last-launch.log" >nul 2>&1
if not "%SSA_LAUNCH_EXIT%"=="0" (
    echo Serious Sam Advance exited with code %SSA_LAUNCH_EXIT%.
    if exist "%SSA_RUN_LOG%" type "%SSA_RUN_LOG%"
    echo Error log: "%SSA_RUN_LOG%"
    if not "%SSA_LAUNCH_NO_PAUSE%"=="1" pause
)
popd
exit /b %SSA_LAUNCH_EXIT%
