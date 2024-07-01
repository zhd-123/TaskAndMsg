@echo off
cls
setlocal enabledelayedexpansion

call setenv.bat
echo ---start release ts---
set ROOT_DIR=%~dp0..\
set TRANSLATION_PATH=%ROOT_DIR%\assets\ts\
set BUILD_OUTPUT=%ROOT_DIR%\assets\i18n\
set tssuffix=.ts
set qmsuffix=.qm

if not exist %BUILD_OUTPUT% (
	mkdir %BUILD_OUTPUT%
)


for /r %TRANSLATION_PATH% %%i in (*.ts) do (
	set tspath=%%i
	set qmpath=!tspath:%tssuffix%=%qmsuffix%!
	lrelease.exe !tspath! -qm !qmpath!
	move !qmpath! %BUILD_OUTPUT%
)
