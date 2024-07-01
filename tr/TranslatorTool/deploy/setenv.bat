:: execute Visual Studio 2019 vcvarsall.bat
set VS160COMNTOOLS=C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\Common7\Tools
IF "%VisualStudioVersion%"=="" (
    call "%VS160COMNTOOLS%\..\..\VC\Auxiliary\Build\vcvarsall.bat" x86 > nul 2>nul
)
echo VisualStudio version is: %VisualStudioVersion%

:: see if Visual Studio 2019 found
IF "%VisualStudioVersion%"=="" (
    echo ERROR: can't find Visual Studio 2019
    exit /b 1
)
IF NOT "%VisualStudioVersion%"=="16.0" (
    echo ERROR: MUST BE Visual Studio 2019
    exit /b 1
)

:: execute qtenv2.bat
IF exist "D:\Qt\5.15.2\msvc2019\bin\qtenv2.bat" (
    set QTPATH=D:\Qt\5.15.2
)

set "QTENV=%QTPATH%\msvc2019\bin\qtenv2.bat"
set "BUILDCMD=%QTPATH%\..\Tools\QtCreator\bin\jom\jom.exe"
set "QMAKECMD=%QTPATH%\msvc2019\bin\qmake.exe"
set "WINDEPLOYQT=%QTPATH%\msvc2019\bin\windeployqt.exe"

if "%QTENV%"=="" (
	echo ERROR: can't find qtenv2.bat
    exit /b 1
)
call "%QTENV%"