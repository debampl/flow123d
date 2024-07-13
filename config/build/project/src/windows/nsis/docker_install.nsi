
Var DOCKER_PATH
Var DOCKER_EXE
!define DOCKER_HK   "Software\Docker Inc.\Docker\1.0"

Var DockerDialog
Var DockerLabel
Var DockerCheckBox
Var DockerCheckBox_State
Var ImageCtrl
Var DockerBmpHandle

Function DockerPage

  SetOutPath $INSTDIR
  File /r "win"

  SetRegView 64
  ReadRegStr $DOCKER_PATH HKLM "${DOCKER_HK}" "AppPath"
  DetailPrint "DOCKER_PATH $DOCKER_PATH"
  ${If} $DOCKER_PATH != ""
    StrCpy $DOCKER_EXE "$DOCKER_PATH\Docker Desktop.exe"
    DetailPrint "DOCKER_EXE $DOCKER_EXE"
  ${Endif}
  

  !insertmacro MUI_HEADER_TEXT "Docker Desktop" "Docker Desktop not found on your system."
  
  nsDialogs::Create 1018
  Pop $DockerDialog

  ${If} $DockerDialog == error
    Abort
  ${EndIf}

  ${NSD_CreateBitmap} 60u 40u 100% 100% ""
	Pop $ImageCtrl
	${NSD_SetBitmap} $ImageCtrl $INSTDIR\win\docker.bmp $DockerBmpHandle

  # if Docker not found, show Docker page
  ${If} $DOCKER_PATH == ""
      DetailPrint "Docker Desktop not found. Install?"

      ${NSD_CreateLabel} 0 0 100% 25u "Docker Desktop not found. Flow123d is run inside container, Docker is therefore necessary.$\r$\nDo you want Flow123d to try installing Docker Desktop now?"
      Pop $DockerLabel

      ; ${NSD_CreateButton} 0 60u 100% 25u "Install now."
      ; Pop $DockerButton

      ; ${NSD_OnClick} $DockerButton DockerInstall

      ${NSD_CreateCheckbox} 60u 100u 100% 20u "Install Docker Desktop now."
      Pop $DockerCheckBox

      ; DetailPrint "[no] Do not install Docker now."
      ;     MessageBox MB_OK|MB_ICONINFORMATION "Please install Docker manually (https://hub.docker.com/) and run Flow123d installator again."
      ;     Abort

      ; disable install button
      ; GetDlgItem $1 $HWNDPARENT 1 
      ; EnableWindow $1 0
  ${Else}
      DetailPrint "Docker Desktop found."
      !insertmacro MUI_HEADER_TEXT "Docker Desktop" "Docker Desktop is already present on your system."

      ${NSD_CreateLabel} 0 0 100% 25u "The installator will try to pull the Flow123d image in next step. Make sure you have a proper internet connection."
      Pop $DockerLabel
  ${Endif}

  nsDialogs::Show
  ${NSD_FreeBitmap} $DockerBmpHandle
FunctionEnd

Function DockerPageLeave
  ${If} $DOCKER_PATH == ""
    ${NSD_GetState} $DockerCheckBox $DockerCheckBox_State

    ${If} $DockerCheckBox_State == 0
      MessageBox MB_ABORTRETRYIGNORE|MB_ICONEXCLAMATION|MB_DEFBUTTON2 "Flow123d cannot be used without Docker. If you installed Docker engine manually (without Docker Desktop), you can still proceed by 'Ignore', just make sure Docker deamon is running. Otherwise 'Abort' the installation or return to previous page by 'Retry'" IDRETRY  MB_retry IDIGNORE MB_ignore
    ${Else}
      Call DockerInstall  
    ${EndIf}
    
    ; quit on Abort button
    Quit
    ; return to page on Retry button
    MB_retry:
      Abort
    ; proceed with the installation on Ignore button
    MB_ignore:
  ${EndIf}
FunctionEnd

Function DockerInstall
  
  DetailPrint "Try installing Docker now."
  MessageBox MB_OK|MB_ICONINFORMATION "Docker installation may require logout or restart.$\r$\nPlease run Flow123d installator once again afterwards."

  # try installing Docker
  DetailPrint "Downloading Docker for Windows installer"
  # download the file
  SetOutPath "$INSTDIR\win"
  ExecWait '"powershell" -ExecutionPolicy RemoteSigned -File "$INSTDIR\win\download-dfw.ps1"'
  # run the installer
  ExecWait '"$INSTDIR\win\dfw.exe"'

  # check whether the installation was succesfull
  ; ReadRegStr $DOCKER_EXE HKLM "${DOCKER_HK}" "AppPath"
  ; DetailPrint "DOCKER_EXE $DOCKER_EXE"

  ; ${If} $DOCKER_EXE == ""
  ;   MessageBox MB_OK|MB_ICONSTOP "Docker not found, installation failed.$\r$\nPlease try installing Docker manually (https://hub.docker.com/)."
  ; ${Endif}
  ; Sleep 2222
  SetOutPath $INSTDIR
  RMDir /r "$INSTDIR"
  Quit
FunctionEnd

Function REMOVE_OLD
  # detect previous installation and warn user
  ClearErrors
  ReadRegStr $0 HKCU "Environment" "DOCKER_CERT_PATH"
  ${If} ${Errors}
    # MessageBox MB_OK "no Docker Toolbox found"
  ${Else}
    # Docker Toolbox found
    MessageBox MB_OK "Detected previous installation of Docker Toolbox, you may need to restart your PC later on"
    # do not set the flag for now
    # SetRebootFlag true
  ${EndIf}

  # remove Docker Toolbox env variables
  DeleteRegValue HKCU "Environment" "DOCKER_CERT_PATH"
  DeleteRegValue HKCU "Environment" "DOCKER_HOST"
  DeleteRegValue HKCU "Environment" "DOCKER_MACHINE_NAME"
  DeleteRegValue HKCU "Environment" "DOCKER_TLS_VERIFY"
  DeleteRegValue HKCU "Environment" "DOCKER_TOOLBOX_INSTALL_PATH"
FunctionEnd

; Function CHECK_DOCKER
;   SetRegView 64
;   ReadRegStr $DOCKER_EXE HKLM "${DOCKER_HK}" "AppPath"
;   DetailPrint "DOCKER_EXE $DOCKER_EXE"

;   ${If} $DOCKER_EXE == ""
;     DetailPrint "Docker not found. Install?"

;     # ask whether install Docker automatically
;     MessageBox MB_YESNO|MB_ICONQUESTION "Docker not found.$\r$\nDo you want Flow123d to try installing Docker now?" IDYES docker_install_true IDNO docker_install_false
;     docker_install_true:
;         DetailPrint "[yes] Try installing Docker now."
;         MessageBox MB_OK|MB_ICONINFORMATION "Docker installation will require logout or restart.$\r$\nPlease run Flow123d installator once again afterwards."
;         Goto docker_install
;     docker_install_false:
;         DetailPrint "[no] Do not install Docker now."
;         MessageBox MB_OK|MB_ICONINFORMATION "Please install Docker manually (https://hub.docker.com/) and run Flow123d installator again."
;         Abort

;     # try installing Docker
;     docker_install:
;     DetailPrint "Downloading Docker for Windows installer"
;     # download the file
;     SetOutPath "$INSTDIR\win"
;     ExecWait '"powershell" -ExecutionPolicy RemoteSigned -File "$INSTDIR\win\download-dfw.ps1"'
;     # run the installer
;     ExecWait '"$INSTDIR\win\dfw.exe"'

;     # check whether the installation was succesfull
;     ReadRegStr $DOCKER_EXE HKLM "${DOCKER_HK}" "AppPath"
;     DetailPrint "DOCKER_EXE $DOCKER_EXE"

;     ${If} $DOCKER_EXE == ""
;       MessageBox MB_OK|MB_ICONSTOP "Docker not found, installation failed.$\r$\nPlease try installing Docker manually (https://hub.docker.com/)."
;     ${Endif}

;     Abort
;   ${Endif}
; FunctionEnd


; install Docker engine without Docker Desktop on Windows
; Docker engine itself does not require license
; https://betterprogramming.pub/how-to-install-docker-without-docker-desktop-on-windows-a2bbb65638a1
