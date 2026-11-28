; 墨记 MoJi · Inno Setup 6 安装脚本
; 生成：MoJi-Setup-1.2.0.exe（含 unins000.exe）
; 编译：ISCC.exe installer\moji.iss

#define AppName "墨记 MoJi"
#define AppVersion "1.2.0"
#define AppPublisher "LiBertY6IO"
#define AppURL "https://github.com/liumouren2006/Electronic_Notepad"
#define AppExe "moji-gui.exe"

[Setup]
AppId={{8F3C2A51-6D4B-4E8A-9C21-7B5D0E6A1F42}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}
VersionInfoVersion=1.2.0.0
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppName} 电子记事本
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#AppVersion}
DefaultDirName={localappdata}\Programs\MoJi
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
DisableDirPage=no
UsePreviousAppDir=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\dist\installer
OutputBaseFilename=MoJi-Setup-{#AppVersion}
SetupIconFile=moji.ico
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName}
LicenseFile=..\LICENSE
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
ChangesEnvironment=no
AllowNoIcons=yes
SetupLogging=yes

[Languages]
Name: "chinesesimplified"; MessagesFile: "ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加任务："
Name: "addtopath"; Description: "把安装目录加入用户 PATH（终端可用 moji 命令）"; GroupDescription: "附加任务："; Flags: checkedonce

[Files]
Source: "..\build\moji.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\moji-gui.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\build\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "..\build\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "..\build\iconengines\*"; DestDir: "{app}\iconengines"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "..\build\tls\*"; DestDir: "{app}\tls"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "..\build\networkinformation\*"; DestDir: "{app}\networkinformation"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "..\build\generic\*"; DestDir: "{app}\generic"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "..\build\platforminputcontexts\*"; DestDir: "{app}\platforminputcontexts"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\需求对照.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\开发日志.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\docs\*"; DestDir: "{app}\docs"; Flags: ignoreversion recursesubdirs createallsubdirs

Source: "..\c_legacy\notebook.c"; DestDir: "{app}\c_legacy"; Flags: ignoreversion
Source: "..\c_legacy\notebook.exe"; DestDir: "{app}\c_legacy"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\c_legacy\README.md"; DestDir: "{app}\c_legacy"; Flags: ignoreversion
Source: "..\c_legacy\build.bat"; DestDir: "{app}\c_legacy"; Flags: ignoreversion
Source: "..\c_legacy\build.sh"; DestDir: "{app}\c_legacy"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"; WorkingDir: "{app}"; Comment: "墨记 MoJi 电子记事本"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "启动 {#AppName}"; Flags: nowait postinstall skipifsilent

[Code]
const
  EnvKey = 'Environment';

function PathHasDir(const Path, Dir: string): Boolean;
var
  Rest, Part: string;
  P: Integer;
begin
  Result := False;
  Rest := Path;
  while Rest <> '' do
  begin
    P := Pos(';', Rest);
    if P = 0 then
    begin
      Part := Rest;
      Rest := '';
    end
    else
    begin
      Part := Copy(Rest, 1, P - 1);
      Rest := Copy(Rest, P + 1, Length(Rest));
    end;
    if CompareText(Trim(Part), Dir) = 0 then
    begin
      Result := True;
      Exit;
    end;
  end;
end;

procedure AddToPath();
var
  OldPath, NewPath, AppDir: string;
begin
  AppDir := ExpandConstant('{app}');
  if not RegQueryStringValue(HKEY_CURRENT_USER, EnvKey, 'Path', OldPath) then
    OldPath := '';
  if not PathHasDir(OldPath, AppDir) then
  begin
    if OldPath = '' then
      NewPath := AppDir
    else
      NewPath := OldPath + ';' + AppDir;
    RegWriteExpandStringValue(HKEY_CURRENT_USER, EnvKey, 'Path', NewPath);
  end;
end;

procedure RemoveFromPath();
var
  OldPath, NewPath, AppDir, Rest, Part: string;
  P: Integer;
begin
  AppDir := ExpandConstant('{app}');
  if not RegQueryStringValue(HKEY_CURRENT_USER, EnvKey, 'Path', OldPath) then
    Exit;
  NewPath := '';
  Rest := OldPath;
  while Rest <> '' do
  begin
    P := Pos(';', Rest);
    if P = 0 then
    begin
      Part := Rest;
      Rest := '';
    end
    else
    begin
      Part := Copy(Rest, 1, P - 1);
      Rest := Copy(Rest, P + 1, Length(Rest));
    end;
    if (Part <> '') and (CompareText(Trim(Part), AppDir) <> 0) then
    begin
      if NewPath = '' then
        NewPath := Part
      else
        NewPath := NewPath + ';' + Part;
    end;
  end;
  RegWriteExpandStringValue(HKEY_CURRENT_USER, EnvKey, 'Path', NewPath);
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) and WizardIsTaskSelected('addtopath') then
    AddToPath();
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  DataDir: string;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    RemoveFromPath();
    if not UninstallSilent then
    begin
      DataDir := ExpandConstant('{userprofile}\.moji');
      if DirExists(DataDir) then
      begin
        if MsgBox('是否同时删除记事数据？' + #13#10 + DataDir + #13#10 +
                  '选择“否”会保留所有记事和附件。',
                  mbConfirmation, MB_YESNO) = IDYES then
          DelTree(DataDir, True, True, True);
      end;
    end;
  end;
end;
