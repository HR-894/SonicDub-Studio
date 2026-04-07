@echo off
REM ══════════════════════════════════════════════════════════════════════════════
REM  Build AI Bridge Server as standalone .exe using PyInstaller
REM  This eliminates the Python dependency for end-users.
REM  Output: ai_engine/dist/ai_bridge_server/ai_bridge_server.exe
REM ══════════════════════════════════════════════════════════════════════════════

echo [SonicDubStudio] Building AI Bridge Server...

REM Ensure PyInstaller is installed
pip install pyinstaller

REM Bundle the FastAPI server + all dependencies into a single folder
pyinstaller ^
    --name ai_bridge_server ^
    --distpath ai_engine ^
    --workpath ai_engine/build ^
    --specpath ai_engine ^
    --noconfirm ^
    --console ^
    ai_engine/server.py

echo.
echo [SonicDubStudio] Build complete!
echo   Output: ai_engine\ai_bridge_server\ai_bridge_server.exe
echo   Copy this folder alongside SonicDubStudio.exe for deployment.
