@echo off
rem Opens the editor directly on this project (avoids the "missing modules" prompt seen when
rem the engine is launched without a project and auto-loads the recent one).
start "" "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0PacShark.uproject"
