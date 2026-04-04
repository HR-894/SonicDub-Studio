; VideoDubber NSIS Installer Script
; Requires: NSIS 3.x, install from https://nsis.sourceforge.io

!define APP_NAME       "VideoDubber"
!define APP_VERSION    "1.0.0"
!define APP_PUBLISHER  "VideoDubber"
!define APP_EXE        "VideoDubber.exe"
!define APP_ICON       "resources\app.ico"
!define INSTALL_DIR    "$PROGRAMFILES64\${APP_NAME}"
!define REGKEY         "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"

Name "${APP_NAME} ${APP_VERSION}"
OutFile "VideoDubber-${APP_VERSION}-Setup.exe"
InstallDir "${INSTALL_DIR}"
InstallDirRegKey HKLM "${REGKEY}" "InstallLocation"
RequestExecutionLevel admin

SetCompressor lzma

!include "MUI2.nsh"
!include "LogicLib.nsh"

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "${APP_ICON}"
!define MUI_UNICON "${APP_ICON}"
!define MUI_WELCOMEPAGE_TITLE "Welcome to ${APP_NAME} Setup"
!define MUI_WELCOMEPAGE_TEXT "This will install ${APP_NAME} ${APP_VERSION} on your computer."
!define MUI_FINISHPAGE_RUN "$INSTDIR\${APP_EXE}"
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\README.txt"

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "LICENSE.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; Components
Section "!${APP_NAME} (required)" SEC_MAIN
  SectionIn RO
  SetOutPath "$INSTDIR"
  
  File "build\Release\${APP_EXE}"
  File "config.json"
  File "README.md"

  ; Qt DLLs
  File "build\Release\Qt6Core.dll"
  File "build\Release\Qt6Gui.dll"
  File "build\Release\Qt6Widgets.dll"
  File "build\Release\Qt6Network.dll"
  
  ; FFmpeg DLLs
  File "build\Release\avcodec-61.dll"
  File "build\Release\avformat-61.dll"
  File "build\Release\avutil-59.dll"
  File "build\Release\avfilter-10.dll"
  File "build\Release\swresample-5.dll"
  
  ; Runtime
  File "build\Release\libcurl.dll"

  ; Create app data dir
  CreateDirectory "$APPDATA\VideoDubber"
  CreateDirectory "$APPDATA\VideoDubber\temp"
  CreateDirectory "$APPDATA\VideoDubber\logs"
  CreateDirectory "$APPDATA\VideoDubber\models"
  CreateDirectory "$PROFILE\Videos\Dubbed"

  ; Start menu
  CreateDirectory "$SMPROGRAMS\${APP_NAME}"
  CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"
  CreateShortcut "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk"   "$INSTDIR\Uninstall.exe"
  CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"

  ; Registry
  WriteRegStr HKLM "${REGKEY}" "DisplayName"     "${APP_NAME}"
  WriteRegStr HKLM "${REGKEY}" "DisplayVersion"  "${APP_VERSION}"
  WriteRegStr HKLM "${REGKEY}" "Publisher"       "${APP_PUBLISHER}"
  WriteRegStr HKLM "${REGKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${REGKEY}" "DisplayIcon"     "$INSTDIR\${APP_EXE}"
  WriteRegStr HKLM "${REGKEY}" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegDWORD HKLM "${REGKEY}" "NoModify" 1
  WriteRegDWORD HKLM "${REGKEY}" "NoRepair"  1

  WriteUninstaller "$INSTDIR\Uninstall.exe"
SectionEnd

Section "Whisper Model (medium, 1.5GB)" SEC_MODEL
  SetOutPath "$APPDATA\VideoDubber\models"
  ; Download at install time using NSISdl
  NSISdl::download \
    "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium.bin" \
    "$APPDATA\VideoDubber\models\ggml-medium.bin"
  Pop $R0
  ${If} $R0 != "success"
    MessageBox MB_OK "Model download failed. Download manually from:$\nhttps://huggingface.co/ggerganov/whisper.cpp"
  ${EndIf}
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\*.*"
  RMDir /r "$INSTDIR\platforms"
  RMDir /r "$INSTDIR\imageformats"
  RMDir "$INSTDIR"
  Delete "$DESKTOP\${APP_NAME}.lnk"
  RMDir /r "$SMPROGRAMS\${APP_NAME}"
  DeleteRegKey HKLM "${REGKEY}"
SectionEnd
