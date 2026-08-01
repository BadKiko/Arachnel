; Fast UI-only Inno build - QML-like monochrome (iterate via Arachnel-UI-Test.exe)
; Full payload: later merge into Arachnel.iss

#define MyAppName "Arachnel"
#ifndef MyAppVersion
  #define MyAppVersion "ui-test"
#endif

[Setup]
AppId={{A8E3C1B2-4D5F-6A70-8B9C-0D1E2F3A4B5C}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName}
DefaultDirName={localappdata}\Programs\Arachnel
DisableProgramGroupPage=yes
DisableReadyPage=yes
DisableWelcomePage=yes
DisableFinishedPage=yes
OutputDir=output
OutputBaseFilename=Arachnel-UI-Test
SetupIconFile=..\..\resources\icons\arachnel.ico
Compression=none
SolidCompression=no
WizardStyle=classic
PrivilegesRequired=lowest
ShowLanguageDialog=no
Uninstallable=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"

[Files]
Source: "skin\background.bmp"; Flags: dontcopy
Source: "skin\path-frame.bmp"; Flags: dontcopy
Source: "skin\pill-filled.png"; Flags: dontcopy
Source: "skin\pill-outline.png"; Flags: dontcopy
Source: "skin\pill-filled-wide.png"; Flags: dontcopy
Source: "skin\pill-outline-wide.png"; Flags: dontcopy
Source: "skin\pill-ghost.png"; Flags: dontcopy
Source: "skin\btn-browse.png"; Flags: dontcopy
Source: "skin\botva2.dll"; Flags: dontcopy
Source: "skin\InnoCallback.dll"; Flags: dontcopy
Source: "skin\ui-dummy.txt"; DestDir: "{app}"; Flags: ignoreversion

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
procedure ExitProcess(uExitCode: UINT);
  external 'ExitProcess@kernel32.dll stdcall';

var
  BackImage, PathFrame: TBitmapImage;
  TitleLabel, BodyLabel, PathFloatLabel, ShortcutsLabel: TNewStaticText;
  DeskLabel, StartLabel: TNewStaticText;
  PathEdit: TNewEdit;
  DeskCheck, StartCheck: TNewCheckBox;
  LangPage: TWizardPage;
  hEnFill, hEnOut, hRuFill, hRuOut: HWND;
  hNext, hBack, hBrowse: HWND;
  LangIsRu: Boolean;
  SkinReady: Boolean;
  UiPhase: Integer;

procedure RefreshPhase; forward;
procedure GoNext; forward;
procedure GoBack; forward;
procedure SyncLangPills; forward;

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
  ExtractTemporaryFile('botva2.dll');
  ExtractTemporaryFile('InnoCallback.dll');
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
  WizardForm.NextButton.Visible := False;
  WizardForm.BackButton.Visible := False;
  WizardForm.CancelButton.Visible := False;
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
  { Selected = filled + dark text; unselected = outline + light text }
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

procedure GoNext;
begin
  if UiPhase < 2 then begin
    Inc(UiPhase);
    RefreshPhase;
  end else begin
    MsgBox('UI test OK'#13#10 + PathEdit.Text, mbInformation, MB_OK);
    ExitProcess(0);
  end;
end;

procedure GoBack;
begin
  if UiPhase > 0 then begin
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

procedure RefreshPhase;
var
  OnLang, OnIntro, OnDir: Boolean;
begin
  if not SkinReady then Exit;
  OnLang := UiPhase = 0;
  OnIntro := UiPhase = 1;
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
  BtnSetVisibility(hBack, not OnLang);
  BtnSetVisibility(hNext, True);

  case UiPhase of
    0:
      begin
        if LangIsRu then begin
          TitleLabel.Caption := 'Выбор языка';
          BodyLabel.Caption := 'Выберите язык установщика.';
          BtnSetText(hNext, 'Далее');
        end else begin
          TitleLabel.Caption := 'Choose language';
          BodyLabel.Caption := 'Select the installer language.';
          BtnSetText(hNext, 'Continue');
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

procedure InitializeWizard;
var
  WideFilled, WideOutline, PillFilled, PillGhost, BrowseImg: AnsiString;
begin
  ExtractSkin;
  LangIsRu := True;
  UiPhase := 0;

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
  PathEdit.Text := ExpandConstant('{localappdata}\Programs\Arachnel');
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
  PillGhost := ExpandConstant('{tmp}\pill-ghost.png');
  BrowseImg := ExpandConstant('{tmp}\btn-browse.png');

  { Four pills: filled/outline x En/Ru - toggle visibility for correct contrast }
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
  hNext := BtnCreate(WizardForm.Handle, WndW - 150, WndH - 56, 120, 40, PillFilled, 0, False);
  BtnSetFont(hBack, WizardForm.Font.Handle);
  BtnSetFont(hNext, WizardForm.Font.Handle);
  BtnSetFontColor(hBack, COnSurface, COnSurface, COnSurface, $646464);
  BtnSetFontColor(hNext, COnFilled, COnFilled, COnFilled, $787878);
  BtnSetEvent(hBack, BtnClickEventID, WrapBtnCallback(@OnBack, 1));
  BtnSetEvent(hNext, BtnClickEventID, WrapBtnCallback(@OnNext, 1));

  SkinReady := True;
  HideStd;
  SyncLangPills;
  RefreshPhase;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  HideStd;
  RefreshPhase;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := False;
  GoNext;
end;

function BackButtonClick(CurPageID: Integer): Boolean;
begin
  Result := False;
  GoBack;
end;

procedure DeinitializeSetup;
begin
  if SkinReady then
    gdipShutdown;
end;
