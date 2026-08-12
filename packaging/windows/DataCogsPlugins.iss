; Inno Setup script for the DataCogs Plugins suite (Windows).
; Compiled by packaging/windows/build-installer.ps1, which stages the
; payload and passes the defines below. Inno gives us a proper uninstaller
; in Add/Remove Programs for free.
;
; Defines (passed via ISCC /D...):
;   Version   e.g. 0.1.0
;   StageDir  staged payload root (VST3\, AAX\, IR\ subdirectories)
;   RepoRoot  repo checkout root (for LICENSE)

[Setup]
AppId={{8E1B2A44-4E0C-4C93-9AF6-DataCogsPlug}}
AppName=DataCogs Plugins
AppVersion={#Version}
AppPublisher=DataCogs
AppPublisherURL=https://datacogs.com
AppSupportURL=https://github.com/DataCogs/DataCogsPlugins
DefaultDirName={autopf}\DataCogs Plugins
DisableDirPage=yes
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
LicenseFile={#RepoRoot}\LICENSE
OutputBaseFilename=DataCogs Plugins-{#Version}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=DataCogs Plugins

[Components]
Name: "plugins"; Description: "DataCogs plugins (VST3 + AAX)"; Types: full compact custom; Flags: fixed
Name: "irlibrary"; Description: "Impulse response library for DataCogs Reverb"; Types: full

[Files]
; VST3 bundles -> the standard system VST3 folder
Source: "{#StageDir}\VST3\*"; DestDir: "{commoncf64}\VST3"; \
    Flags: recursesubdirs ignoreversion; Components: plugins
; AAX bundles -> Avid's shared plug-in folder
Source: "{#StageDir}\AAX\*"; DestDir: "{commoncf64}\Avid\Audio\Plug-Ins"; \
    Flags: recursesubdirs ignoreversion; Components: plugins
; IR library -> ProgramData (the reverb's system-wide search root;
; per-user libraries in Documents always shadow it)
Source: "{#StageDir}\IR\*"; DestDir: "{commonappdata}\DataCogs\Impulse Responses"; \
    Flags: recursesubdirs ignoreversion; Components: irlibrary
