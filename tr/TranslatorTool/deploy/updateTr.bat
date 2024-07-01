@echo off
CHCP 936
cls
setlocal enabledelayedexpansion

call setenv.bat
echo ---start update ts---
echo %~dp0
set ROOT_DIR=%~dp0..\
set TRANSLATION_PATH=%ROOT_DIR%\assets\ts\
set PRO_FILE=%ROOT_DIR%\editor.pro
set suffix=.ts
set langList=de en es fr it ja ko nl pt ru zh zhhk zhyz
set targetLangList=de-DE en-US es-ES fr-FR it-IT ja-JP ko-KR nl-NL pt-PT ru-RU zh-CN zh-CN zh-CN
if not exist %TRANSLATION_PATH% (
	mkdir %TRANSLATION_PATH%
)

for %%a in (%langList%) do ( 
   set tsfile=%%a%suffix%
   for %%i in (%targetLangList%) do (
		set language=%%i
		echo !language! | findstr %%a >nul && (
			set target=!language!
		)
   )
   lupdate.exe %PRO_FILE% -no-ui-lines -locations none -source-language en-US -target-language !target! -ts %TRANSLATION_PATH%!tsfile!
)

echo --
echo --
echo -- 生成翻译文件，需要先转为utf-8格式
echo -- 再运行releaseTr.bat
echo --
echo --
