; Per-user candidate installer. Installed-file lists remove only application files.
!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "TextFunc.nsh"
!include "StrFunc.nsh"
${StrStr}
${UnStrStr}
!ifndef STAGE
  !error "Pass -DSTAGE=<verified payload>"
!endif
!ifndef OUTPUT
  !error "Pass -DOUTPUT=<versioned installer>"
!endif
!ifndef CANDIDATE_VERSION
  !error "Pass -DCANDIDATE_VERSION=<PrismVersion.txt>"
!endif
Unicode true
Name "Prism ${CANDIDATE_VERSION}"
OutFile "${OUTPUT}"
RequestExecutionLevel user
InstallDir "$LOCALAPPDATA\Programs\Prism"
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

!macro RequireClosed PREFIX
Function ${PREFIX}CheckClosed
  IfFileExists "$INSTDIR\prism.exe" 0 done
  System::Call 'kernel32::CreateFileW(w "$INSTDIR\prism.exe", i 0x40000000, i 0, p 0, i 3, i 0x80, p 0) p.r0'
  IntCmp $0 -1 blocked
  System::Call 'kernel32::CloseHandle(p r0)'
  Goto done
  blocked:
    MessageBox MB_OK|MB_ICONEXCLAMATION "Close Prism and save your work before installing or uninstalling this candidate."
    Abort
  done:
FunctionEnd
!macroend
!insertmacro RequireClosed ""
!insertmacro RequireClosed "un."
Function un.onInit
  Call un.CheckClosed
FunctionEnd

; Validate installed relative paths before removing anything: no traversal,
; absolute paths or wildcards. Leave projects/settings/recordings untouched.
!macro RemoveOwned PREFIX
Function ${PREFIX}RemoveOwned
  Push $0
  Push $1
  Push $2
  Push $3
  StrCpy $3 0
  nextList:
    StrCmp $3 0 0 directories
    FileOpen $0 "$INSTDIR\installed-files.txt" r
    Goto opened
  directories:
    FileOpen $0 "$INSTDIR\installed-directories.txt" r
  opened:
    IfErrors nextMode
    ; Lists are generated as UTF-16LE with BOM by package.ps1.
    FileSeek $0 2 SET
  nextPath:
    ClearErrors
    FileReadUTF16LE $0 $1
    IfErrors closeList
    ${TrimNewLines} $1 $1
    StrCmp $1 "" nextPath
    !insertmacro STRFUNC_CALL_StrStr "${PREFIX}" $2 $1 ".."
    StrCmp $2 "" 0 closeList
    !insertmacro STRFUNC_CALL_StrStr "${PREFIX}" $2 $1 ":"
    StrCmp $2 "" 0 closeList
    !insertmacro STRFUNC_CALL_StrStr "${PREFIX}" $2 $1 "*"
    StrCmp $2 "" 0 closeList
    !insertmacro STRFUNC_CALL_StrStr "${PREFIX}" $2 $1 "?"
    StrCmp $2 "" 0 closeList
    StrCpy $2 $1 1
    StrCmp $2 "\" closeList
    StrCmp $2 "/" closeList
    StrCmp $1 "installed-files.txt" nextPath
    StrCmp $1 "installed-directories.txt" nextPath
    StrCmp $3 0 0 removeDirectory
    Delete "$INSTDIR\$1"
    Goto nextPath
  removeDirectory:
    RMDir "$INSTDIR\$1"
    Goto nextPath
  closeList:
    FileClose $0
  nextMode:
    IntOp $3 $3 + 1
    StrCmp $3 1 nextList
    Delete "$INSTDIR\installed-files.txt"
    Delete "$INSTDIR\installed-directories.txt"
  Pop $3
  Pop $2
  Pop $1
  Pop $0
FunctionEnd
!macroend
!insertmacro RemoveOwned ""
!insertmacro RemoveOwned "un."

Section "Prism" SecPrism
  ; Close Prism and save work before upgrading; file copy errors must be visible.
  Call CheckClosed
  Call RemoveOwned
  SetOutPath $INSTDIR
  File /r "${STAGE}\*.*"
  CreateDirectory "$SMPROGRAMS\Prism"
  CreateShortcut "$SMPROGRAMS\Prism\Prism.lnk" "$INSTDIR\prism.exe"
  CreateShortcut "$SMPROGRAMS\Prism\Uninstall Prism.lnk" "$INSTDIR\Uninstall.exe"
  CreateShortcut "$DESKTOP\Prism.lnk" "$INSTDIR\prism.exe"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Prism" "DisplayName" "Prism ${CANDIDATE_VERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Prism" "DisplayVersion" "${CANDIDATE_VERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Prism" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Prism" "DisplayIcon" "$INSTDIR\prism.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Prism" "InstallLocation" "$INSTDIR"
SectionEnd

Section "Uninstall"
  Call un.RemoveOwned
  Delete "$DESKTOP\Prism.lnk"
  Delete "$SMPROGRAMS\Prism\Prism.lnk"
  Delete "$SMPROGRAMS\Prism\Uninstall Prism.lnk"
  RMDir "$SMPROGRAMS\Prism"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Prism"
  ; Deliberately never touch AppData or recurse into unknown installation files.
SectionEnd
