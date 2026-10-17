@echo off
"%~dp0build\native-windows\Release\ssa_native_viewer.exe" "%~dp0orig\SSA.gba" %*
if errorlevel 1 pause
