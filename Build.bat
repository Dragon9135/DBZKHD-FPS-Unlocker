@echo off
echo ================================================
echo  Dragon Ball Z: Kakarot (HD) FPS Unlocker Build
echo ================================================
echo.

REM 1. Compile the resource file
echo [1/2] Compiling: Version.rc
windres Version.rc -O coff -o Version.o
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Resource compilation failed!
    pause
    exit /b 1
)

REM 2. Compile and link the main code
echo [2/2] Compiling: Main.cpp + Version.o
g++ -shared -O2 -o "FPS Unlocker for DBZK (HD).asi" Main.cpp Version.o -lpsapi -static-libgcc -static-libstdc++ -DPSAPI_VERSION=1 -DWIN32_LEAN_AND_MEAN -DNOMINMAX
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Main code compilation failed!
    pause
    exit /b 1
)

REM 3. Cleaning
del Version.o

echo.
echo ===================================================
echo  SUCCESS! Output: “FPS Unlocker for DBZK (HD).asi”
echo ===================================================
pause