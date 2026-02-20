@echo off
setlocal EnableExtensions

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

if not defined VCPKG_ROOT (
  echo ERROR: VCPKG_ROOT is not set.
  echo        Temporary: set "VCPKG_ROOT=C:\vcpkg"
  exit /b 1
)

set "TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
if not exist "%TOOLCHAIN%" (
  echo ERROR: vcpkg toolchain not found: "%TOOLCHAIN%"
  exit /b 1
)

set "BUILD_DIR=%ROOT%\build\msvc-x64"
set "CONFIG=Debug"
set "TRIPLET=x64-windows"

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%" >nul 2>&1

echo Configuring...
cmake -S "%ROOT%" -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE:FILEPATH="%TOOLCHAIN%" ^
  -DVCPKG_TARGET_TRIPLET=%TRIPLET% ^
  -DVKMINI_ENABLE_VALIDATION=ON
if errorlevel 1 exit /b 1

echo Building (%CONFIG%)...
cmake --build "%BUILD_DIR%" --config "%CONFIG%"
if errorlevel 1 exit /b 1

set "CACHE=%BUILD_DIR%\CMakeCache.txt"
if not exist "%CACHE%" (
  echo ERROR: Missing CMakeCache: "%CACHE%"
  exit /b 1
)

for /f "tokens=2 delims==" %%B in ('findstr /b /c:"VCPKG_INSTALLED_DIR:PATH=" "%CACHE%"') do set "VCPKG_INSTALLED_DIR=%%B"
if not defined VCPKG_INSTALLED_DIR (
  for /f "tokens=2 delims==" %%B in ('findstr /b /c:"VCPKG_INSTALLED_DIR:INTERNAL=" "%CACHE%"') do set "VCPKG_INSTALLED_DIR=%%B"
)
if not defined VCPKG_INSTALLED_DIR (
  echo ERROR: Could not read VCPKG_INSTALLED_DIR from "%CACHE%"
  exit /b 1
)

set "INST=%VCPKG_INSTALLED_DIR%\%TRIPLET%"
if not exist "%INST%\" (
  echo ERROR: Triplet dir missing: "%INST%"
  exit /b 1
)

set "LAYER_JSON=%INST%\bin\VkLayer_khronos_validation.json"
if exist "%LAYER_JSON%" (
  set "VK_LAYER_PATH=%INST%\bin"
  set "VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation"
  rem vcpkg note: VK_ADD_LAYER_PATH also works with the loader
  set "VK_ADD_LAYER_PATH=%INST%\bin"
  set "PATH=%VK_LAYER_PATH%;%PATH%"
  echo.
  echo Validation enabled:
  echo   VK_LAYER_PATH      = %VK_LAYER_PATH%
  echo   VK_ADD_LAYER_PATH  = %VK_ADD_LAYER_PATH%
  echo   VK_INSTANCE_LAYERS = %VK_INSTANCE_LAYERS%
  echo.
) else (
  echo.
  echo WARNING: Validation layer json not found at:
  echo   "%LAYER_JSON%"
  echo   Validation will not be enabled.
  echo.
)

set "EXE=%BUILD_DIR%\%CONFIG%\vulkan_app.exe"
if not exist "%EXE%" (
  echo ERROR: exe not found: "%EXE%"
  exit /b 1
)

echo Running: "%EXE%"
"%EXE%"
exit /b 0
