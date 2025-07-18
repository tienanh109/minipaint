[Setup]
AppName=Mini Paint
AppVersion=1.1
WizardStyle=modern
SetupIconFile=icon.ico
DefaultDirName={autopf}\Mini Paint
DefaultGroupName=Mini Paint
UninstallDisplayIcon={app}\MiniPaint.exe
Compression=lzma2
SolidCompression=yes
OutputDir=userdocs:MiniPaintSetupOutput
OutputBaseFilename=MiniPaint_AutoArch

[Files]
; tienanh109
Source: "MiniPaint_x86.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall
Source: "MiniPaint_x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Icons]
Name: "{group}\Mini Paint"; Filename: "{app}\MiniPaint.exe"
Name: "{group}\Uninstall Mini Paint"; Filename: "{uninstallexe}"

[Code]
function Is64BitInstallMode: Boolean;
begin
  Result := (ProcessorArchitecture = paX64) or (ProcessorArchitecture = paARM64);
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  SrcFile, DstFile: string;
begin
  if CurStep = ssInstall then
  begin
    DstFile := ExpandConstant('{app}\MiniPaint.exe');
    if Is64BitInstallMode then
      SrcFile := ExpandConstant('{tmp}\MiniPaint_x64.exe')
    else
      SrcFile := ExpandConstant('{tmp}\MiniPaint_x86.exe');

    FileCopy(SrcFile, DstFile, False);
  end;
end;
