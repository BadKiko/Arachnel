; Arachnel shipping installer. UI = Arachnel-UI.iss (known-good). Install via ReadyToInstall + Next.OnClick.
; Build: .\setup\inno\pack-inno.ps1

#define MyAppName "Arachnel"
#define MyAppExeName "arachnel_app.exe"
#define MyAppPublisher "Arachnel"
#define MyAppURL "https://github.com/Arachnel"
#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif

[Setup]
AppId={{A8E3C1B2-4D5F-6A70-8B9C-0D1E2F3A4B5C}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
; Prefer previous Inno dir, else old Qt SFX InstallLocation, else per-user Programs.
DefaultDirName={code:GetDefaultDir}
DefaultGroupName={#MyAppName}
UsePreviousAppDir=yes
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=yes
DisableWelcomePage=yes
AllowNoIcons=yes
OutputDir=output
OutputBaseFilename=Arachnel-{#MyAppVersion}-Setup
SetupIconFile=..\..\resources\icons\arachnel.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2
SolidCompression=no
WizardStyle=classic
PrivilegesRequired=lowest
; Elevate when target is Program Files (legacy SFX installs) or /ALLUSERS is passed.
PrivilegesRequiredOverridesAllowed=dialog commandline
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
CloseApplications=yes
RestartApplications=no
ShowLanguageDialog=no
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription={#MyAppName} Setup
VersionInfoProductName={#MyAppName}
VersionInfoVersion=0.0.0.0
; Only launcher files under {app}. Game libraries / settings live in AppData - never wipe those.

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"

[Files]
Source: "skin\background.bmp"; Flags: dontcopy nocompression
Source: "skin\path-frame.bmp"; Flags: dontcopy nocompression
Source: "skin\pill-filled.png"; Flags: dontcopy nocompression
Source: "skin\pill-outline.png"; Flags: dontcopy nocompression
Source: "skin\pill-filled-wide.png"; Flags: dontcopy nocompression
Source: "skin\pill-outline-wide.png"; Flags: dontcopy nocompression
Source: "skin\pill-ghost.png"; Flags: dontcopy nocompression
Source: "skin\btn-browse.png"; Flags: dontcopy nocompression
Source: "skin\botva2.dll"; Flags: dontcopy nocompression
Source: "skin\InnoCallback.dll"; Flags: dontcopy nocompression
Source: "..\..\dist-win\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

; Leftover from the old Qt SFX installer - not part of Inno payload.
[InstallDelete]
Type: files; Name: "{app}\uninstall.exe"
Type: files; Name: "{app}\arachnel_setup.exe"
Type: files; Name: "{app}\arachnel_setup_launcher.exe"

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Check: WantStartMenuIcon
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"; Check: WantStartMenuIcon
Name: "{userdesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Check: WantDesktopIcon

[Registry]
; Deep link: arachnel://game/<entryId> (share links use https://discover.badkiko.ru/open/game/<id>)
Root: HKCU; Subkey: "Software\Classes\arachnel"; ValueType: string; ValueName: ""; ValueData: "URL:Arachnel Protocol"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\arachnel"; ValueType: string; ValueName: "URL Protocol"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\arachnel\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""

[Run]
; In-app update uses /SILENT - relaunch Arachnel when unpack finishes.
; Interactive wizard has its own Launch button (skipifnotsilent).
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifnotsilent

[Code]
const
  WndW = 560;
  WndH = 460;
  BtnClickEventID = 1;
  COnSurface = $FFFFFF;
  CMuted = $B0B0B0;
  CSurface = $121214;
  COnFilled = $121214;

type
  TBtnEventProc = procedure(h: HWND);
  TTimerProc = procedure(hWnd: LongWord; uMsg, idEvent, dwTime: LongWord);

function BtnCreate(hParent: HWND; Left, Top, Width, Height: Integer; FileName: PAnsiChar; ShadowWidth: Integer; IsCheckBtn: Boolean): HWND;
  external 'BtnCreate@files:botva2.dll stdcall delayload';
procedure BtnSetText(h: HWND; Text: PAnsiChar);
  external 'BtnSetText@files:botva2.dll stdcall delayload';
procedure BtnSetFont(h: HWND; AFont: Cardinal);
  external 'BtnSetFont@files:botva2.dll stdcall delayload';
procedure BtnSetFontColor(h: HWND; NormalColor, FocusColor, PressedColor, DisabledColor: Cardinal);
  external 'BtnSetFontColor@files:botva2.dll stdcall delayload';
procedure BtnSetEvent(h: HWND; EventID: Integer; Event: Longword);
  external 'BtnSetEvent@files:botva2.dll stdcall delayload';
procedure BtnSetVisibility(h: HWND; Value: Boolean);
  external 'BtnSetVisibility@files:botva2.dll stdcall delayload';
procedure gdipShutdown;
  external 'gdipShutdown@files:botva2.dll stdcall delayload';
function WrapBtnCallback(Callback: TBtnEventProc; ParamCount: Integer): Longword;
  external 'wrapcallback@files:InnoCallback.dll stdcall delayload';
function WrapTimerCallback(Callback: TTimerProc; ParamCount: Integer): Longword;
  external 'wrapcallback@files:InnoCallback.dll stdcall delayload';

const
  SC_CLOSE = $F060;
  MF_BYCOMMAND = $0;
  ARM_TIMER_ID = 77;

var
  BackImage, PathFrame: TBitmapImage;
  TitleLabel, BodyLabel, PathFloatLabel, ShortcutsLabel: TNewStaticText;
  DeskLabel, StartLabel: TNewStaticText;
  PathEdit: TNewEdit;
  DeskCheck, StartCheck: TNewCheckBox;
  LangPage: TWizardPage;
  hEnFill, hEnOut, hRuFill, hRuOut: HWND;
  hNext, hBack, hBrowse, hCancel, hLaunch: HWND;
  LangIsRu: Boolean;
  SkinReady: Boolean;
  UiPhase: Integer;
  ReadyToInstall: Boolean;
  WantDesk, WantStart: Boolean;
  InstallArmed: Boolean;
  FinishPending: Boolean;
  ArmTries: Integer;
  ArmTimerId: LongWord;
  ArmTimerProc: LongWord;

procedure RefreshPhase; forward;
procedure GoNext; forward;
procedure GoBack; forward;
procedure SyncLangPills; forward;
procedure ArmPump; forward;

function WantDesktopIcon: Boolean;
begin
  Result := WantDesk;
end;

function WantStartMenuIcon: Boolean;
begin
  Result := WantStart;
end;

{ Old Qt SFX wrote HKLM/HKCU\...\Uninstall\Arachnel (no Inno AppId). }
function LegacySfxInstallDir: String;
var
  Dir: String;
begin
  Dir := '';
  if not RegQueryStringValue(HKLM,
       'Software\Microsoft\Windows\CurrentVersion\Uninstall\Arachnel',
       'InstallLocation', Dir) then
    RegQueryStringValue(HKCU,
       'Software\Microsoft\Windows\CurrentVersion\Uninstall\Arachnel',
       'InstallLocation', Dir);
  Dir := RemoveBackslashUnlessRoot(Trim(Dir));
  if (Dir <> '') and FileExists(AddBackslash(Dir) + '{#MyAppExeName}') then
    Result := Dir
  else if (Dir <> '') and DirExists(Dir) then
    Result := Dir
  else
    Result := '';
end;

function GetDefaultDir(Param: String): String;
var
  Legacy: String;
begin
  Legacy := LegacySfxInstallDir;
  if Legacy <> '' then
    Result := Legacy
  else
    Result := ExpandConstant('{localappdata}\Programs\{#MyAppName}');
end;

procedure RemoveLegacySfxUninstallKey;
begin
  // Drop old Apps & Features entry so users don't see two Arachnels.
  // Do NOT run the old uninstall.exe - that could wipe more than the launcher folder.
  RegDeleteKeyIncludingSubkeys(HKLM,
    'Software\Microsoft\Windows\CurrentVersion\Uninstall\Arachnel');
  RegDeleteKeyIncludingSubkeys(HKCU,
    'Software\Microsoft\Windows\CurrentVersion\Uninstall\Arachnel');
end;

function PostMessage(hWnd: HWND; Msg, wParam, lParam: Longint): LongBool;
  external 'PostMessageW@user32.dll stdcall';
function GetSystemMenu(hWnd: HWND; bRevert: Boolean): LongWord;
  external 'GetSystemMenu@user32.dll stdcall';
function EnableMenuItem(hMenu: LongWord; uIDEnableItem, uEnable: LongWord): Boolean;
  external 'EnableMenuItem@user32.dll stdcall';
function SetTimer(hWnd: LongWord; nIDEvent, uElapse, lpTimerFunc: LongWord): LongWord;
  external 'SetTimer@user32.dll stdcall';
function KillTimer(hWnd: LongWord; nIDEvent: LongWord): LongBool;
  external 'KillTimer@user32.dll stdcall';

procedure ExtractSkin;
begin
  ExtractTemporaryFile('background.bmp');
  ExtractTemporaryFile('path-frame.bmp');
  ExtractTemporaryFile('pill-filled.png');
  ExtractTemporaryFile('pill-outline.png');
  ExtractTemporaryFile('pill-filled-wide.png');
  ExtractTemporaryFile('pill-outline-wide.png');
  ExtractTemporaryFile('pill-ghost.png');
  ExtractTemporaryFile('btn-browse.png');
end;

procedure ParkCancel;
begin
  WizardForm.CancelButton.Visible := True;
  WizardForm.CancelButton.Enabled := True;
  WizardForm.CancelButton.TabStop := False;
  WizardForm.CancelButton.Left := -2000;
  WizardForm.CancelButton.Top := -1800;
  WizardForm.CancelButton.Width := 75;
  WizardForm.CancelButton.Height := 23;
  { Inno grays SC_CLOSE from Cancel.CanFocus at page change; re-enable after our hide }
  EnableMenuItem(GetSystemMenu(WizardForm.Handle, False), SC_CLOSE, MF_BYCOMMAND);
end;

procedure ParkNext;
begin
  WizardForm.NextButton.Visible := True;
  WizardForm.NextButton.Enabled := True;
  WizardForm.NextButton.TabStop := False;
  WizardForm.NextButton.Left := -2000;
  WizardForm.NextButton.Top := -2000;
  WizardForm.NextButton.Width := 75;
  WizardForm.NextButton.Height := 23;
end;

procedure HideStd;
begin
  WizardForm.OuterNotebook.Hide;
  WizardForm.InnerNotebook.Hide;
  WizardForm.Bevel.Hide;
  WizardForm.Bevel1.Hide;
  WizardForm.WizardBitmapImage.Visible := False;
  WizardForm.WizardSmallBitmapImage.Visible := False;
  WizardForm.PageNameLabel.Visible := False;
  WizardForm.PageDescriptionLabel.Visible := False;
  WizardForm.BackButton.Visible := False;
  { Keep Next parked while arming install or finishing - OnClick needs a live button }
  if InstallArmed or FinishPending or (WizardForm.CurPageID = wpFinished) then
    ParkNext
  else
    WizardForm.NextButton.Visible := False;
  ParkCancel;
end;

procedure StyleLbl(L: TNewStaticText; Sz, Col: Integer; Bold: Boolean);
begin
  L.Font.Name := 'Segoe UI';
  L.Font.Size := Sz;
  L.Font.Color := Col;
  if Bold then L.Font.Style := [fsBold] else L.Font.Style := [];
end;

procedure SyncLangPills;
begin
  if not SkinReady then Exit;
  BtnSetVisibility(hEnFill, (UiPhase = 0) and (not LangIsRu));
  BtnSetVisibility(hEnOut, (UiPhase = 0) and LangIsRu);
  BtnSetVisibility(hRuFill, (UiPhase = 0) and LangIsRu);
  BtnSetVisibility(hRuOut, (UiPhase = 0) and (not LangIsRu));
end;

procedure OnLangEn(hBtn: HWND);
begin
  LangIsRu := False;
  WizardForm.Caption := 'Arachnel Setup';
  SyncLangPills;
  RefreshPhase;
end;

procedure OnLangRu(hBtn: HWND);
begin
  LangIsRu := True;
  WizardForm.Caption := 'Установка Arachnel';
  SyncLangPills;
  RefreshPhase;
end;

procedure OnBrowse(hBtn: HWND);
var
  Dir: String;
begin
  Dir := PathEdit.Text;
  if BrowseForFolder('', Dir, False) then
    PathEdit.Text := Dir;
end;

procedure StopArmTimer;
begin
  if ArmTimerId <> 0 then
  begin
    KillTimer(WizardForm.Handle, ARM_TIMER_ID);
    ArmTimerId := 0;
  end;
end;

procedure ScheduleArmTimer(IntervalMs: LongWord);
begin
  StopArmTimer;
  ArmTimerId := SetTimer(WizardForm.Handle, ARM_TIMER_ID, IntervalMs, ArmTimerProc);
end;

procedure ArmPump;
begin
  if WizardForm.CurPageID = wpInstalling then
  begin
    InstallArmed := False;
    StopArmTimer;
    Log('ArmPump reached installing');
    Exit;
  end;
  if WizardForm.CurPageID = wpFinished then
  begin
    { Install already done - do not treat as arm target }
    InstallArmed := False;
    StopArmTimer;
    Log('ArmPump hit finished while arming install');
    Exit;
  end;
  Inc(ArmTries);
  if ArmTries > 24 then
  begin
    InstallArmed := False;
    StopArmTimer;
    Log('ArmPump gave up page=' + IntToStr(WizardForm.CurPageID));
    if LangIsRu then
      MsgBox('Не удалось начать установку. Закройте другие копии Arachnel и попробуйте снова.', mbError, MB_OK)
    else
      MsgBox('Could not start installation. Close other Arachnel copies and try again.', mbError, MB_OK);
    Exit;
  end;
  ParkNext;
  Log('ArmPump click try=' + IntToStr(ArmTries) + ' page=' + IntToStr(WizardForm.CurPageID) +
      ' dir=' + WizardForm.DirEdit.Text);
  WizardForm.NextButton.OnClick(WizardForm.NextButton);
  if (WizardForm.CurPageID <> wpInstalling) and (WizardForm.CurPageID <> wpFinished) then
    ScheduleArmTimer(80)
  else begin
    InstallArmed := False;
    StopArmTimer;
  end;
end;

procedure RequestFinish;
begin
  FinishPending := True;
  ParkNext;
  ScheduleArmTimer(50);
  Log('RequestFinish scheduled');
end;

procedure OnArmTimer(hWnd: LongWord; uMsg, idEvent, dwTime: LongWord);
begin
  ArmTimerId := 0;
  if FinishPending then
  begin
    FinishPending := False;
    ParkNext;
    Log('Finish Next.OnClick page=' + IntToStr(WizardForm.CurPageID));
    WizardForm.NextButton.OnClick(WizardForm.NextButton);
    Exit;
  end;
  ArmPump;
end;

procedure ArmRealNext;
begin
  FinishPending := False;
  InstallArmed := True;
  ParkNext;
  ScheduleArmTimer(50);
  Log('ArmRealNext scheduled dir=' + WizardForm.DirEdit.Text);
end;

procedure BeginInstall;
var
  Dir: String;
begin
  if InstallArmed then
    Exit;
  Dir := Trim(PathEdit.Text);
  if Dir = '' then
  begin
    if LangIsRu then
      MsgBox('Укажите папку установки.', mbError, MB_OK)
    else
      MsgBox('Choose an install folder.', mbError, MB_OK);
    Exit;
  end;
  WantDesk := DeskCheck.Checked;
  WantStart := StartCheck.Checked;
  PathEdit.Text := Dir;
  WizardForm.DirEdit.Text := Dir;
  ForceDirectories(Dir);
  ReadyToInstall := True;
  ArmTries := 0;
  Log('BeginInstall ReadyToInstall dir=' + Dir);
  ArmRealNext;
end;

procedure GoNext;
begin
  if UiPhase < 2 then
  begin
    Inc(UiPhase);
    RefreshPhase;
  end
  else if UiPhase = 2 then
    BeginInstall
  else if UiPhase = 4 then
    RequestFinish;
end;

procedure GoBack;
begin
  if (UiPhase > 0) and (UiPhase < 3) then
  begin
    Dec(UiPhase);
    RefreshPhase;
  end;
end;

procedure OnNext(hBtn: HWND);
begin
  GoNext;
end;

procedure OnBack(hBtn: HWND);
begin
  GoBack;
end;

procedure OnCancel(hBtn: HWND);
begin
  { On finished Cancel.OnClick is disconnected by Inno - finish via Next instead }
  if WizardForm.CurPageID = wpFinished then
    RequestFinish
  else begin
    ParkCancel;
    PostMessage(WizardForm.CancelButton.Handle, $00F5, 0, 0);
  end;
end;

procedure OnLaunch(hBtn: HWND);
var
  Exe: String;
  Err: Integer;
begin
  Exe := AddBackslash(WizardDirValue) + '{#MyAppExeName}';
  if not FileExists(Exe) then
    Exe := ExpandConstant('{app}\{#MyAppExeName}');
  if FileExists(Exe) then
    ShellExec('', Exe, '', ExtractFilePath(Exe), SW_SHOWNORMAL, ewNoWait, Err);
end;

procedure RefreshPhase;
var
  OnLang, OnDir: Boolean;
begin
  if not SkinReady then Exit;
  OnLang := UiPhase = 0;
  OnDir := UiPhase = 2;

  SyncLangPills;

  PathFrame.Visible := OnDir;
  PathEdit.Visible := OnDir;
  PathFloatLabel.Visible := OnDir;
  ShortcutsLabel.Visible := OnDir;
  DeskCheck.Visible := OnDir;
  StartCheck.Visible := OnDir;
  DeskLabel.Visible := OnDir;
  StartLabel.Visible := OnDir;
  BtnSetVisibility(hBrowse, OnDir);
  BtnSetVisibility(hBack, (UiPhase = 1) or OnDir);
  BtnSetVisibility(hNext, UiPhase <> 3);
  BtnSetVisibility(hCancel, UiPhase < 3);
  BtnSetVisibility(hLaunch, UiPhase = 4);

  case UiPhase of
    0:
      begin
        if LangIsRu then begin
          TitleLabel.Caption := 'Выбор языка';
          BodyLabel.Caption := 'Выберите язык установщика.';
          BtnSetText(hNext, 'Далее');
          BtnSetText(hCancel, 'Отмена');
        end else begin
          TitleLabel.Caption := 'Choose language';
          BodyLabel.Caption := 'Select the installer language.';
          BtnSetText(hNext, 'Continue');
          BtnSetText(hCancel, 'Cancel');
        end;
        BodyLabel.Visible := True;
      end;
    1:
      begin
        if LangIsRu then begin
          TitleLabel.Caption := 'Установка Arachnel';
          BodyLabel.Caption := 'Лаунчер игр с плагинными источниками. Мастер распакует Arachnel на ваш компьютер.';
          BtnSetText(hNext, 'Далее');
          BtnSetText(hBack, 'Назад');
        end else begin
          TitleLabel.Caption := 'Install Arachnel';
          BodyLabel.Caption := 'Game launcher with plugin-based sources. This wizard unpacks Arachnel to your computer.';
          BtnSetText(hNext, 'Continue');
          BtnSetText(hBack, 'Back');
        end;
        BodyLabel.Visible := True;
      end;
    2:
      begin
        if LangIsRu then begin
          TitleLabel.Caption := 'Выберите папку установки';
          PathFloatLabel.Caption := 'Папка установки';
          ShortcutsLabel.Caption := 'Ярлыки';
          DeskLabel.Caption := 'Создать ярлык на рабочем столе';
          StartLabel.Caption := 'Создать ярлык в меню Пуск';
          BtnSetText(hNext, 'Установить');
          BtnSetText(hBack, 'Назад');
        end else begin
          TitleLabel.Caption := 'Choose install location';
          PathFloatLabel.Caption := 'Install folder';
          ShortcutsLabel.Caption := 'Shortcuts';
          DeskLabel.Caption := 'Create desktop shortcut';
          StartLabel.Caption := 'Create Start Menu shortcut';
          BtnSetText(hNext, 'Install');
          BtnSetText(hBack, 'Back');
        end;
        BodyLabel.Visible := False;
      end;
    3:
      begin
        if LangIsRu then
          TitleLabel.Caption := 'Установка...'
        else
          TitleLabel.Caption := 'Installing...';
        BodyLabel.Visible := False;
        WizardForm.ProgressGauge.Parent := WizardForm;
        WizardForm.StatusLabel.Parent := WizardForm;
        WizardForm.ProgressGauge.SetBounds(32, 200, 496, 12);
        WizardForm.StatusLabel.SetBounds(32, 170, 496, 20);
        WizardForm.StatusLabel.Font.Color := CMuted;
        WizardForm.ProgressGauge.Visible := True;
        WizardForm.StatusLabel.Visible := True;
      end;
    4:
      begin
        WizardForm.ProgressGauge.Visible := False;
        WizardForm.StatusLabel.Visible := False;
        if LangIsRu then begin
          TitleLabel.Caption := 'Arachnel готов';
          BodyLabel.Caption := WizardDirValue;
          BtnSetText(hLaunch, 'Запустить');
          BtnSetText(hNext, 'Готово');
        end else begin
          TitleLabel.Caption := 'Arachnel is ready';
          BodyLabel.Caption := WizardDirValue;
          BtnSetText(hLaunch, 'Launch');
          BtnSetText(hNext, 'Finish');
        end;
        BodyLabel.Visible := True;
      end;
  end;

  TitleLabel.BringToFront;
  BodyLabel.BringToFront;
  PathFloatLabel.BringToFront;
  PathEdit.BringToFront;
  ShortcutsLabel.BringToFront;
  DeskLabel.BringToFront;
  StartLabel.BringToFront;
  DeskCheck.BringToFront;
  StartCheck.BringToFront;
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  { Silent uses stock Inno pages (no Botva). Inno already drives /SILENT itself. }
  if WizardSilent then
  begin
    Result := False;
    Exit;
  end;
  Result := (PageID = wpSelectDir) or (PageID = wpSelectTasks) or
            (PageID = wpSelectProgramGroup);
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  if WizardSilent then
  begin
    Result := True;
    Exit;
  end;
  Log('NextButtonClick page=' + IntToStr(CurPageID) +
      ' lang=' + IntToStr(LangPage.ID) +
      ' ready=' + IntToStr(Ord(ReadyToInstall)) +
      ' phase=' + IntToStr(UiPhase));
  if CurPageID = wpFinished then
  begin
    Result := True;
    Exit;
  end;
  if ReadyToInstall then
  begin
    WizardForm.DirEdit.Text := Trim(PathEdit.Text);
    Log('Starting install to ' + WizardForm.DirEdit.Text);
    Result := True;
    Exit;
  end;
  Result := False;
end;

function BackButtonClick(CurPageID: Integer): Boolean;
begin
  { Botva owns Back; stock Back must not move pages underneath the skin. }
  Result := WizardSilent;
end;

procedure FinishedCancelClick(Sender: TObject);
begin
  { Inno disconnects Cancel.OnClick on wpFinished; close via Finish instead }
  RequestFinish;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if WizardSilent or not SkinReady then
    Exit;
  HideStd;
  if CurPageID = wpInstalling then
  begin
    UiPhase := 3;
    InstallArmed := False;
    FinishPending := False;
    StopArmTimer;
  end
  else if CurPageID = wpFinished then
  begin
    UiPhase := 4;
    InstallArmed := False;
    FinishPending := False;
    StopArmTimer;
    ParkCancel;
    ParkNext;
    WizardForm.CancelButton.OnClick := @FinishedCancelClick;
  end
  else if ReadyToInstall and InstallArmed and
          (CurPageID <> wpInstalling) and (CurPageID <> wpFinished) then
    ScheduleArmTimer(80);
  RefreshPhase;
end;

procedure CancelButtonClick(CurPageID: Integer; var Cancel, Confirm: Boolean);
begin
  if CurPageID = wpFinished then
  begin
    Confirm := False;
    Cancel := True;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    RemoveLegacySfxUninstallKey;
    Log('PostInstall app=' + ExpandConstant('{app}') + ' ver={#MyAppVersion}');
  end;
end;

procedure InitializeWizard;
var
  WideFilled, WideOutline, PillFilled, PillOutline, PillGhost, BrowseImg: AnsiString;
begin
  LangIsRu := True;
  UiPhase := 0;
  ReadyToInstall := False;
  InstallArmed := False;
  FinishPending := False;
  ArmTries := 0;
  ArmTimerId := 0;
  WantDesk := False;
  WantStart := True;
  SkinReady := False;
  LangPage := nil;

  { In-app update (/SILENT): skip Botva skin. Stock Inno progress window + [Run] relaunch. }
  if WizardSilent then
  begin
    Log('Silent update: stock progress UI');
    Exit;
  end;

  ExtractSkin;
  ArmTimerProc := WrapTimerCallback(@OnArmTimer, 4);

  WizardForm.ClientWidth := WndW;
  WizardForm.ClientHeight := WndH;
  WizardForm.Color := CSurface;
  WizardForm.Font.Name := 'Segoe UI';
  WizardForm.Font.Color := COnSurface;
  WizardForm.Caption := 'Установка Arachnel';

  BackImage := TBitmapImage.Create(WizardForm);
  BackImage.Parent := WizardForm;
  BackImage.SetBounds(0, 0, WndW, WndH);
  BackImage.Stretch := True;
  BackImage.Bitmap.LoadFromFile(ExpandConstant('{tmp}\background.bmp'));
  BackImage.SendToBack;

  TitleLabel := TNewStaticText.Create(WizardForm);
  TitleLabel.Parent := WizardForm;
  TitleLabel.SetBounds(32, 28, 420, 36);
  StyleLbl(TitleLabel, 18, COnSurface, True);

  BodyLabel := TNewStaticText.Create(WizardForm);
  BodyLabel.Parent := WizardForm;
  BodyLabel.SetBounds(32, 70, 400, 70);
  BodyLabel.AutoSize := False;
  StyleLbl(BodyLabel, 10, CMuted, False);

  PathFrame := TBitmapImage.Create(WizardForm);
  PathFrame.Parent := WizardForm;
  PathFrame.SetBounds(32, 96, 420, 44);
  PathFrame.Bitmap.LoadFromFile(ExpandConstant('{tmp}\path-frame.bmp'));
  PathFrame.Visible := False;

  PathFloatLabel := TNewStaticText.Create(WizardForm);
  PathFloatLabel.Parent := WizardForm;
  PathFloatLabel.SetBounds(48, 86, 140, 16);
  StyleLbl(PathFloatLabel, 8, CMuted, False);
  PathFloatLabel.Visible := False;

  PathEdit := TNewEdit.Create(WizardForm);
  PathEdit.Parent := WizardForm;
  PathEdit.SetBounds(44, 104, 396, 28);
  PathEdit.Text := GetDefaultDir('');
  PathEdit.Font.Name := 'Segoe UI';
  PathEdit.Font.Size := 10;
  PathEdit.Font.Color := COnSurface;
  PathEdit.Color := CSurface;
  PathEdit.Visible := False;

  ShortcutsLabel := TNewStaticText.Create(WizardForm);
  ShortcutsLabel.Parent := WizardForm;
  ShortcutsLabel.SetBounds(32, 170, 300, 22);
  StyleLbl(ShortcutsLabel, 11, COnSurface, True);
  ShortcutsLabel.Visible := False;

  DeskCheck := TNewCheckBox.Create(WizardForm);
  DeskCheck.Parent := WizardForm;
  DeskCheck.SetBounds(32, 204, 22, 22);
  DeskCheck.Caption := '';
  DeskCheck.Checked := False;
  DeskCheck.Visible := False;

  DeskLabel := TNewStaticText.Create(WizardForm);
  DeskLabel.Parent := WizardForm;
  DeskLabel.SetBounds(58, 206, 400, 20);
  StyleLbl(DeskLabel, 10, COnSurface, False);
  DeskLabel.Visible := False;

  StartCheck := TNewCheckBox.Create(WizardForm);
  StartCheck.Parent := WizardForm;
  StartCheck.SetBounds(32, 236, 22, 22);
  StartCheck.Caption := '';
  StartCheck.Checked := True;
  StartCheck.Visible := False;

  StartLabel := TNewStaticText.Create(WizardForm);
  StartLabel.Parent := WizardForm;
  StartLabel.SetBounds(58, 238, 400, 20);
  StyleLbl(StartLabel, 10, COnSurface, False);
  StartLabel.Visible := False;

  LangPage := CreateCustomPage(wpWelcome, '', '');

  WideFilled := ExpandConstant('{tmp}\pill-filled-wide.png');
  WideOutline := ExpandConstant('{tmp}\pill-outline-wide.png');
  PillFilled := ExpandConstant('{tmp}\pill-filled.png');
  PillOutline := ExpandConstant('{tmp}\pill-outline.png');
  PillGhost := ExpandConstant('{tmp}\pill-ghost.png');
  BrowseImg := ExpandConstant('{tmp}\btn-browse.png');

  hEnOut := BtnCreate(WizardForm.Handle, 40, 150, 480, 48, WideOutline, 0, False);
  hEnFill := BtnCreate(WizardForm.Handle, 40, 150, 480, 48, WideFilled, 0, False);
  hRuOut := BtnCreate(WizardForm.Handle, 40, 214, 480, 48, WideOutline, 0, False);
  hRuFill := BtnCreate(WizardForm.Handle, 40, 214, 480, 48, WideFilled, 0, False);

  BtnSetText(hEnOut, 'English');
  BtnSetText(hEnFill, 'English');
  BtnSetText(hRuOut, 'Русский');
  BtnSetText(hRuFill, 'Русский');

  BtnSetFont(hEnOut, WizardForm.Font.Handle);
  BtnSetFont(hEnFill, WizardForm.Font.Handle);
  BtnSetFont(hRuOut, WizardForm.Font.Handle);
  BtnSetFont(hRuFill, WizardForm.Font.Handle);

  BtnSetFontColor(hEnOut, COnSurface, COnSurface, COnSurface, $646464);
  BtnSetFontColor(hRuOut, COnSurface, COnSurface, COnSurface, $646464);
  BtnSetFontColor(hEnFill, COnFilled, COnFilled, COnFilled, $787878);
  BtnSetFontColor(hRuFill, COnFilled, COnFilled, COnFilled, $787878);

  BtnSetEvent(hEnOut, BtnClickEventID, WrapBtnCallback(@OnLangEn, 1));
  BtnSetEvent(hEnFill, BtnClickEventID, WrapBtnCallback(@OnLangEn, 1));
  BtnSetEvent(hRuOut, BtnClickEventID, WrapBtnCallback(@OnLangRu, 1));
  BtnSetEvent(hRuFill, BtnClickEventID, WrapBtnCallback(@OnLangRu, 1));

  hBrowse := BtnCreate(WizardForm.Handle, 468, 94, 48, 48, BrowseImg, 0, False);
  BtnSetEvent(hBrowse, BtnClickEventID, WrapBtnCallback(@OnBrowse, 1));

  hBack := BtnCreate(WizardForm.Handle, WndW - 250, WndH - 54, 90, 36, PillGhost, 0, False);
  hCancel := BtnCreate(WizardForm.Handle, 32, WndH - 54, 90, 36, PillGhost, 0, False);
  hLaunch := BtnCreate(WizardForm.Handle, WndW - 280, WndH - 56, 120, 40, PillOutline, 0, False);
  hNext := BtnCreate(WizardForm.Handle, WndW - 150, WndH - 56, 120, 40, PillFilled, 0, False);
  BtnSetFont(hBack, WizardForm.Font.Handle);
  BtnSetFont(hCancel, WizardForm.Font.Handle);
  BtnSetFont(hLaunch, WizardForm.Font.Handle);
  BtnSetFont(hNext, WizardForm.Font.Handle);
  BtnSetFontColor(hBack, COnSurface, COnSurface, COnSurface, $646464);
  BtnSetFontColor(hCancel, COnSurface, COnSurface, COnSurface, $646464);
  BtnSetFontColor(hLaunch, COnSurface, COnSurface, COnSurface, $646464);
  BtnSetFontColor(hNext, COnFilled, COnFilled, COnFilled, $787878);
  BtnSetText(hCancel, 'Отмена');
  BtnSetText(hLaunch, 'Запустить');
  BtnSetVisibility(hLaunch, False);
  BtnSetEvent(hBack, BtnClickEventID, WrapBtnCallback(@OnBack, 1));
  BtnSetEvent(hCancel, BtnClickEventID, WrapBtnCallback(@OnCancel, 1));
  BtnSetEvent(hLaunch, BtnClickEventID, WrapBtnCallback(@OnLaunch, 1));
  BtnSetEvent(hNext, BtnClickEventID, WrapBtnCallback(@OnNext, 1));

  SkinReady := True;
  HideStd;
  SyncLangPills;
  RefreshPhase;
end;

procedure DeinitializeSetup;
begin
  StopArmTimer;
  if SkinReady then
    gdipShutdown;
end;
