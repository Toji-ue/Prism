; Prism per-user installer.
;
; No admin rights needed: everything lands in $LOCALAPPDATA\Programs\Prism and
; the Add/Remove entry is written to HKCU. The payload directory is passed in
; on the makensis command line (-DSTAGE=...) by tools\package.ps1, which
; assembles it from deploy\ (app files + tester README, minus the old script
; installer).
;
; Build: makensis -DSTAGE=<staging dir> tools\installer\prism.nsi

!include "MUI2.nsh"

!ifndef STAGE
  !error "STAGE must point at the staged payload directory (-DSTAGE=...)"
!endif

Name "Prism"
OutFile "..\..\dist\Prism-Setup.exe"
Unicode true
RequestExecutionLevel user
InstallDir "$LOCALAPPDATA\Programs\Prism"

!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Section "Prism" SecPrism
  SetOutPath $INSTDIR
  ; Payload: the app plus its Qt DLLs/plugins/QML and the tester README.
  File /r "${STAGE}\*.*"

  CreateDirectory "$SMPROGRAMS\Prism"
  CreateShortcut "$SMPROGRAMS\Prism\Prism.lnk" "$INSTDIR\prism.exe"
  CreateShortcut "$SMPROGRAMS\Prism\Uninstall Prism.lnk" "$INSTDIR\Uninstall.exe"
  CreateShortcut "$DESKTOP\Prism.lnk" "$INSTDIR\prism.exe"

  WriteUninstaller "$INSTDIR\Uninstall.exe"

  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Prism" "DisplayName" "Prism"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Prism" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Prism" "DisplayIcon" "$INSTDIR\prism.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Prism" "InstallLocation" "$INSTDIR"
SectionEnd

Section "Uninstall"
  ; App plus everything the installer laid down. The uninstaller deletes
  ; itself last (NSIS runs it from a temp copy when needed).
  Delete "$DESKTOP\Prism.lnk"
  RMDir /r "$SMPROGRAMS\Prism"
  RMDir /r "$INSTDIR"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Prism"
SectionEnd
