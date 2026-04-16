@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if %errorlevel% neq 0 exit /b %errorlevel%

echo [SonicDubStudio] Configuring CMake...
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_CUDA=OFF -DBUILD_TESTS=OFF
if %errorlevel% neq 0 exit /b %errorlevel%

echo [SonicDubStudio] Building...
cmake --build build --config Release
if %errorlevel% neq 0 exit /b %errorlevel%

echo [SonicDubStudio] Build successful!
