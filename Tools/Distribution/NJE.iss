#ifndef StageDir
  #error StageDir must point to a prepared NJE package
#endif
#ifndef OutputDir
  #define OutputDir AddBackslash(SourcePath) + "installer-output"
#endif
#ifndef AppVersion
  #define AppVersion GetFileVersion(AddBackslash(StageDir) + "NJE.exe")
#endif
#ifndef AlphaVersion
  #define AlphaVersion "0.1"
#endif
[Setup]
AppId={{B447E46F-5D2A-4F4D-BD08-171D68A665D2}
AppName=NJE
AppVersion={#AppVersion}
AppVerName=NJE Alpha {#AlphaVersion}
AppPublisher=Francisco Ramon Asensi Domingo
AppPublisherURL=https://github.com/Nil0s/NJE-Releases
AppUpdatesURL=https://nil0s.github.io/NJE-Releases/
DefaultDirName={localappdata}\Programs\NJE
DefaultGroupName=NJE
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
DisableProgramGroupPage=yes
UsePreviousAppDir=yes
CloseApplications=no
RestartApplications=no
AppMutex=NJE.Editor,NJE.Trainer,NJE.Runtime
SetupIconFile={#StageDir}\Resources\NoJobEngine.ico
UninstallDisplayIcon={app}\NJE.exe
LicenseFile={#StageDir}\LICENSE
WizardStyle=modern
Compression=lzma2
SolidCompression=yes
OutputDir={#OutputDir}
#ifdef NJETestMode
OutputBaseFilename=NJE_TestSetup_{#AppVersion}
CreateUninstallRegKey=no
#else
OutputBaseFilename=NJE_Alpha_{#AlphaVersion}_Setup
#endif
VersionInfoVersion={#AppVersion}
VersionInfoProductName=NJE Alpha {#AlphaVersion}
[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked
[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Excludes: "NJE.alpha,NoJobEngine.alpha,PackageValidation.txt,ExamplesValidation.txt"; Flags: ignoreversion recursesubdirs createallsubdirs
[INI]
Filename: "{app}\InstalledVersion.ini"; Section: "NJE"; Key: "PublicVersion"; String: "Alpha {#AlphaVersion}"
Filename: "{app}\InstalledVersion.ini"; Section: "NJE"; Key: "Version"; String: "{#AppVersion}"
#ifndef NJETestMode
[Icons]
Name: "{userprograms}\NJE\NJE"; Filename: "{app}\NJE.exe"; WorkingDir: "{app}"; IconFilename: "{app}\NJE.exe"
Name: "{userprograms}\NJE\NJE Trainer"; Filename: "{app}\NJETrainer.exe"; WorkingDir: "{app}"; IconFilename: "{app}\NJETrainer.exe"
Name: "{userdesktop}\NJE"; Filename: "{app}\NJE.exe"; WorkingDir: "{app}"; IconFilename: "{app}\NJE.exe"; Tasks: desktopicon
#endif
[Run]
Filename: "{app}\NJE.exe"; Description: "Launch NJE"; Flags: nowait postinstall skipifsilent
; User projects, models, settings and histories are outside {app}.
; No UninstallDelete or wildcard cleanup targets user data.
