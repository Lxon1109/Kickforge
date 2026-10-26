@echo off
setlocal EnableExtensions
cd /d "%~dp0"
title Kickforge - Windows-Build

echo.
echo  =====================================================
echo    KICKFORGE  -  Windows-Build  [VST3 + Standalone]
echo  =====================================================
echo.

rem ---------------------------------------------------------------
rem  1. Visual Studio mit C++-Werkzeugen finden
rem ---------------------------------------------------------------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :no_vs

rem Ausgabe ueber eine Temp-Datei lesen: "for /f" bricht bei "(x86)" im Pfad gerne
set "VSDIR="
"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "%TEMP%\kickforge_vsdir.txt" 2>nul
set /p VSDIR=<"%TEMP%\kickforge_vsdir.txt"
del "%TEMP%\kickforge_vsdir.txt" >nul 2>nul
if not defined VSDIR goto :no_cpp

echo [1/4] Visual Studio gefunden:
echo       %VSDIR%
call "%VSDIR%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
where cmake >nul 2>nul
if errorlevel 1 goto :no_cmake

rem ---------------------------------------------------------------
rem  2. JUCE 8.0.4 holen (einmalig, ca. 100 MB)
rem ---------------------------------------------------------------
if exist "external\JUCE\CMakeLists.txt" goto :have_juce
echo [2/4] Lade JUCE 8.0.4 herunter ... das dauert einen Moment
if not exist external mkdir external
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ProgressPreference='SilentlyContinue'; Invoke-WebRequest -Uri 'https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.4.zip' -OutFile 'external\juce.zip'; Expand-Archive -Force 'external\juce.zip' 'external'; Remove-Item 'external\juce.zip'; Rename-Item 'external\JUCE-8.0.4' 'JUCE'"
if not exist "external\JUCE\CMakeLists.txt" goto :no_juce
goto :configure
:have_juce
echo [2/4] JUCE ist schon da.

rem ---------------------------------------------------------------
rem  3. Konfigurieren + bauen (Release)
rem ---------------------------------------------------------------
:configure
echo [3/4] Baue Kickforge ... das dauert beim ersten Mal 5-15 Minuten
where ninja >nul 2>nul
if errorlevel 1 (
    cmake -S . -B build -A x64
) else (
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
)
if errorlevel 1 goto :build_failed
cmake --build build --config Release --parallel
if errorlevel 1 goto :build_failed

rem ---------------------------------------------------------------
rem  4. Ergebnis in den Ordner "dist" legen
rem ---------------------------------------------------------------
echo [4/4] Lege die fertigen Dateien in den Ordner "dist"
if exist dist rmdir /s /q dist
mkdir dist
xcopy /e /i /q /y "build\Kickforge_artefacts\Release\VST3\Kickforge.vst3" "dist\Kickforge.vst3" >nul
copy /y "build\Kickforge_artefacts\Release\Standalone\Kickforge.exe" "dist\" >nul
if not exist "dist\Kickforge.vst3" goto :build_failed

rem Hilfsskript fuer die Installation (laeuft mit Admin-Rechten)
> "dist\install-vst3.cmd" echo @echo off
>>"dist\install-vst3.cmd" echo xcopy /e /i /y "%%~dp0Kickforge.vst3" "%%CommonProgramFiles%%\VST3\Kickforge.vst3" ^>nul
>>"dist\install-vst3.cmd" echo if errorlevel 1 ^(echo Installation fehlgeschlagen.^) else ^(echo Kickforge ist installiert.^)
>>"dist\install-vst3.cmd" echo pause

echo.
echo  =====================================================
echo    FERTIG!  Die Dateien liegen in:  %CD%\dist
echo  =====================================================
echo.
choice /c JN /m "Kickforge jetzt fuer FL Studio installieren? Windows fragt dann nach Admin-Rechten"
if errorlevel 2 goto :skip_install
powershell -NoProfile -Command "Start-Process -FilePath '%CD%\dist\install-vst3.cmd' -Verb RunAs -Wait"
echo.
echo In FL Studio jetzt:  Options  -  Manage plugins  -  Find installed plugins
echo Kickforge erscheint dann bei den Instrumenten.
:skip_install
start "" explorer "%CD%\dist"
echo.
pause
exit /b 0

rem ---------------------------------------------------------------
rem  Fehlermeldungen
rem ---------------------------------------------------------------
:no_vs
echo [FEHLER] Visual Studio wurde nicht gefunden.
echo          Bitte "Visual Studio 2022 Community" installieren:
echo          https://visualstudio.microsoft.com/de/vs/community/
echo          Dabei den Workload "Desktopentwicklung mit C++" anhaken.
pause
exit /b 1

:no_cpp
echo [FEHLER] Visual Studio ist da, aber ohne C++-Werkzeuge.
echo          Visual Studio Installer oeffnen - "Aendern" - Workload
echo          "Desktopentwicklung mit C++" anhaken und installieren.
pause
exit /b 1

:no_cmake
echo [FEHLER] CMake fehlt. Visual Studio Installer oeffnen - "Aendern" -
echo          unter "Desktopentwicklung mit C++" die Komponente
echo          "C++-CMake-Tools fuer Windows" anhaken.
pause
exit /b 1

:no_juce
echo [FEHLER] JUCE konnte nicht heruntergeladen werden.
echo          Internetverbindung pruefen und das Skript nochmal starten.
pause
exit /b 1

:build_failed
echo.
echo [FEHLER] Der Build ist fehlgeschlagen.
echo          Mach bitte einen Screenshot von den letzten Zeilen
echo          und schick ihn mir - dann fixe ich das.
pause
exit /b 1
