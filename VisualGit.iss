[Setup]
AppName=Visual Git
AppVersion=1.0
AppCopyright=Copyright (c) 2021 Calvin Buckley
AppPublisher=Calvin Buckley
AppPublisherURL=http://visualgit.io/
DefaultDirName={pf}\Visual Git
DefaultGroupName=Visual Git
DisableProgramGroupPage=yes
ChangesEnvironment=yes
OutputBaseFilename=VisualGitSetup
SetupIconFile=lgit.ico
InfoBeforeFile=SetupReadme.rtf
PrivilegesRequired=admin

[Files]
Source: "LGit.dll"; DestDir: "{app}"
Source: "SwapSCC.exe"; DestDir: "{app}"
Source: "VGit.exe"; DestDir: "{app}"
; Dependencies
Source: "ssleay32.dll"; DestDir: "{app}"
Source: "libeay32.dll"; DestDir: "{app}"
Source: "cert.pem"; DestDir: "{app}"
Source: "zlib1.dll"; DestDir: "{app}"
Source: "git2.dll"; DestDir: "{app}"
Source: "libssh2.dll"; DestDir: "{app}"
; Redists
; (Don't use, just install what we need directly)
;Source: "vcredist.exe"; DestDir: "{app}\redist"
Source: "vcfiles\msvcp60.dll";  DestDir: "{sys}"; OnlyBelowVersion: 0,6; Flags: restartreplace uninsneveruninstall sharedfile
Source: "vcfiles\msvcirt.dll";  DestDir: "{sys}"; OnlyBelowVersion: 0,6; Flags: restartreplace uninsneveruninstall sharedfile
Source: "vcfiles\msvcrt.dll";   DestDir: "{sys}"; OnlyBelowVersion: 0,6; Flags: restartreplace uninsneveruninstall sharedfile
Source: "vcfiles\oleaut32.dll"; DestDir: "{sys}"; OnlyBelowVersion: 0,6; Flags: restartreplace uninsneveruninstall sharedfile regserver
Source: "vcfiles\olepro32.dll"; DestDir: "{sys}"; OnlyBelowVersion: 0,6; Flags: restartreplace uninsneveruninstall sharedfile regserver

;[Run]
;Filename: "{app}\redist\vcredist.exe"; Parameters: "/q"; WorkingDir: {app}\redist; StatusMsg: Installing VC++6 runtime...

[Icons]
Name: "{group}\Visual Git"; Filename: "{app}\VGit.exe"
Name: "{group}\Swap SCC Provider"; Filename: "{app}\SwapSCC.exe"

[Registry]
Root: HKLM; Subkey: "Software\Visual Git"; Flags: uninsdeletekeyifempty
Root: HKLM; Subkey: "Software\Visual Git\Visual Git"; Flags: uninsdeletekey
Root: HKLM; Subkey: "Software\Visual Git\Visual Git"; ValueType: string; ValueName: "SCCServerName"; ValueData: "Visual Git"
Root: HKLM; Subkey: "Software\Visual Git\Visual Git"; ValueType: string; ValueName: "SCCServerPath"; ValueData: "{app}\LGit.dll"
Root: HKLM; Subkey: "Software\Visual Git\Visual Git"; ValueType: string; ValueName: "CertificateBundlePath"; ValueData: "{app}\cert.pem"
Root: HKLM; Subkey: "Software\SourceCodeControlProvider\InstalledSCCProviders"; ValueType: string; ValueName: "Visual Git"; ValueData: "Software\Visual Git\Visual Git"
; XXX: avoid changes if not needed
;Root: HKLM; Subkey: "Software\SourceCodeControlProvider"; ValueType: string; ValueName: "ProviderRegKey"; ValueData: "Software\Visual Git\Visual Git"
; Append to PATH, because we need DLL resolution
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"; \
    ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};{app}"; \
    Check: NeedsAddPath(ExpandConstant('{app}'))
; For a context menu item
Root: HKCR; Subkey: "Directory\shell\visual_git"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\VGit.exe"
Root: HKCR; Subkey: "Directory\shell\visual_git"; ValueType: string; ValueName: ""; ValueData: "Open Visual Git Here"
Root: HKCR; Subkey: "Directory\shell\visual_git\command"; ValueType: string; ValueName: ""; ValueData: """{app}\VGit.exe"" %1\"

[Code]

function NeedsAddPath(Param: string): boolean;
var
  OrigPath: string;
begin
  if not RegQueryStringValue(HKEY_LOCAL_MACHINE,
    'SYSTEM\CurrentControlSet\Control\Session Manager\Environment',
    'Path', OrigPath)
  then begin
    Result := True;
    exit;
  end;
  { look for the path with leading and trailing semicolon }
  { Pos() returns 0 if not found }
  Result := Pos(';' + Param + ';', ';' + OrigPath + ';') = 0;
end;
