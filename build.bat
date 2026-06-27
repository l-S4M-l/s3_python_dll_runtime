@echo off
setlocal

cd /d "%~dp0"

set "BUILD_DIR=build"
set "DIST_DIR=dist"

if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"

echo Configuring with Visual Studio 17 2022 x64...
cmake -S . -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo Visual Studio 2022 configure failed. Trying Visual Studio 16 2019 x64...
    if exist "%BUILD_DIR%\CMakeCache.txt" rmdir /S /Q "%BUILD_DIR%"
    cmake -S . -B "%BUILD_DIR%" -G "Visual Studio 16 2019" -A x64
    if errorlevel 1 (
        echo CMake configure failed.
        exit /b 1
    )
)

echo Building Release...
cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

if exist "%BUILD_DIR%\Release\python_runner.dll" copy /Y "%BUILD_DIR%\Release\python_runner.dll" "%DIST_DIR%\python_runner.dll" >nul
if exist "%BUILD_DIR%\python_runner.dir\Release\python_runner.dll" copy /Y "%BUILD_DIR%\python_runner.dir\Release\python_runner.dll" "%DIST_DIR%\python_runner.dll" >nul

if exist "%DIST_DIR%\py" rmdir /S /Q "%DIST_DIR%\py"
xcopy /E /I /Y "py_template" "%DIST_DIR%\py" >nul

echo.
echo Build complete.
echo.
echo Install:
echo   1. Copy dist\python_runner.dll into GameFolder\Mods
echo   2. Either let the DLL create GameFolder\Mods\py, or copy dist\py to GameFolder\Mods\py
echo   3. Launch the game
echo   4. Check GameFolder\python_runner.log
echo   5. Check GameFolder\python_test_success.txt
echo.

endlocal
