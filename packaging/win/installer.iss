; Compressor Anatomy Lab 2 — Windows インストーラー（Inno Setup 6）
; GitHub Actions から:  iscc /DMyAppVersion=0.2.0 /DArtDir=..\..\build\CompAnatomyLab2_artefacts\Release packaging\win\installer.iss
; 出力: dist\CompAnatomyLab2-<ver>-win64.exe （コード署名なし）

#ifndef MyAppVersion
  #define MyAppVersion "0.2.0"
#endif
#ifndef ArtDir
  #define ArtDir "..\..\build\CompAnatomyLab2_artefacts\Release"
#endif
#define MyAppName "Compressor Anatomy Lab 2"

[Setup]
AppId={{5ECE00E6-4CD2-40D6-80F5-A53C0A3CDC54}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=JPK (by Johan Plus Kei)
AppPublisherURL=https://jpk.seesaa.net/
DefaultDirName={autopf}\JPK\{#MyAppName}
DefaultGroupName=JPK
DisableProgramGroupPage=yes
LicenseFile=license-win.txt
InfoBeforeFile=notice-win.txt
OutputDir=..\..\dist
OutputBaseFilename=CompAnatomyLab2-{#MyAppVersion}-win64
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#MyAppName}

[Languages]
Name: "ja"; MessagesFile: "compiler:Languages\Japanese.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full";   Description: "Full / すべて"
Name: "custom"; Description: "Custom / カスタム"; Flags: iscustom

[Components]
Name: "vst3";       Description: "VST3 plug-in (Live / Cubase / Studio One / FL Studio …)"; Types: full custom
Name: "standalone"; Description: "Standalone app / DAWなしで試せるアプリ";                  Types: full

[Files]
Source: "{#ArtDir}\VST3\{#MyAppName}.vst3\*"; DestDir: "{commoncf64}\VST3\{#MyAppName}.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ArtDir}\Standalone\{#MyAppName}.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppName}.exe"; Components: standalone
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#MyAppName}.vst3"
