# Builds the Windows suite installer: "DataCogs Plugins-<version>.exe".
# The Windows counterpart of packaging/build-installer.sh.
#
# AAX signing: Windows AAX must be signed ON Windows (PACE rule). When the
# PACE Code Signing SDK is installed and PACE_PASSWORD is set, the staged
# AAX bundles are cloud-signed here (iloktool session + wraptool
# --allowsigningservice, mirroring the macOS flow). Without the tools it
# warns loudly and packages unsigned AAX (fine for Pro Tools Developer
# builds; retail Pro Tools needs the signature).
#
# Usage:
#   packaging/windows/build-installer.ps1 -Version 0.1.0 `
#       -IrLibrary ir-library [-BuildDir build] [-OutDir dist]

param(
    [string]$BuildDir = "build",
    [string]$IrLibrary = "$env:USERPROFILE\Documents\DataCogs\Impulse Responses",
    [string]$OutDir = "dist",
    [string]$Version = "0.1.0"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
Set-Location $RepoRoot

function Fail([string]$msg) { Write-Error $msg; exit 1 }

# ---- stage the payload ------------------------------------------------------
$Stage = Join-Path ([System.IO.Path]::GetTempPath()) "datacogs-stage-$(Get-Random)"
foreach ($sub in "VST3", "AAX", "IR") {
    New-Item -ItemType Directory -Force -Path (Join-Path $Stage $sub) | Out-Null
}

# Everything from here runs inside try/finally so the staging directory is
# cleaned up even when a step fails mid-way.
try {

$plugins = @(
    @{ dir = "compressor";        target = "CompressorPlugin" },
    @{ dir = "parametric-eq";     target = "ParametricEQPlugin" },
    @{ dir = "convolution-reverb"; target = "ConvolutionReverbPlugin" }
)

foreach ($p in $plugins) {
    $art = Join-Path $BuildDir "plugins/$($p.dir)/plugin/$($p.target)_artefacts"
    if (Test-Path (Join-Path $art "Release")) { $art = Join-Path $art "Release" }

    $vst3 = Get-ChildItem -Path (Join-Path $art "VST3") -Filter "*.vst3" |
            Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not $vst3) { Fail "no VST3 bundle for $($p.target) under $art" }
    Copy-Item $vst3.FullName -Destination (Join-Path $Stage "VST3") -Recurse

    $aax = Get-ChildItem -Path (Join-Path $art "AAX") -Filter "*.aaxplugin" |
           Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not $aax) { Fail "no AAX bundle for $($p.target) under $art" }
    Copy-Item $aax.FullName -Destination (Join-Path $Stage "AAX") -Recurse
}

if (-not (Test-Path $IrLibrary)) { Fail "IR library not found at $IrLibrary" }
Copy-Item (Join-Path $IrLibrary "*") -Destination (Join-Path $Stage "IR") -Recurse
Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $Stage "IR\favourites.txt")

# ---- PACE cloud-sign the staged AAX (when the tools are here) ---------------
# Tool locations vary between PACE SDK versions - search rather than guess,
# and say exactly what's missing so CI logs are actionable.
function Find-Tool([string]$override, [string]$name, [string[]]$roots) {
    if ($override) { return $override }
    foreach ($root in $roots) {
        $hit = Get-ChildItem -Path $root -Recurse -Filter $name -ErrorAction SilentlyContinue |
               Select-Object -First 1
        if ($hit) { return $hit.FullName }
    }
    return $null
}

$Wraptool = Find-Tool $env:WRAPTOOL "wraptool.exe" @(
    "C:\Program Files\PACEAntiPiracy", "C:\Program Files (x86)\PACEAntiPiracy")
$Iloktool = Find-Tool $env:ILOKTOOL "iloktool.exe" @(
    "C:\Program Files (x86)\iLok License Manager", "C:\Program Files\iLok License Manager",
    "C:\Program Files\PACEAntiPiracy", "C:\Program Files (x86)\PACEAntiPiracy")
$PaceAccount = if ($env:PACE_ACCOUNT) { $env:PACE_ACCOUNT } else { "kogzee" }
$PaceWcguid  = if ($env:PACE_WCGUID)  { $env:PACE_WCGUID }
               else { "FCB93630-951E-11F1-9B0E-00505692AD3E" }

# REQUIRE_SIGNED=1 (set by CI on tag builds): shipping unsigned AAX is a
# hard failure, not a warning. Local/dev builds without it keep the
# warn-and-continue behaviour.
$RequireSigned = $env:REQUIRE_SIGNED -eq "1"

$missing = @()
if (-not $Wraptool) {
    $missing += "wraptool.exe (PACE Code Signing SDK)"
    Write-Warning "wraptool.exe not found under PACEAntiPiracy - dumping what IS installed:"
    Get-ChildItem "C:\Program Files*\PACEAntiPiracy" -Recurse -Depth 3 -ErrorAction SilentlyContinue |
        Select-Object -ExpandProperty FullName | Write-Host
}
if (-not $Iloktool) { $missing += "iloktool.exe (iLok License Support)" }
if (-not $env:PACE_PASSWORD) { $missing += "PACE_PASSWORD environment variable" }

$cloudSession = $false
if ($missing.Count -gt 0 -and $RequireSigned) {
    Fail "REQUIRE_SIGNED is set but AAX signing is impossible - missing: $($missing -join '; ')"
}

if ($Wraptool -and $env:PACE_PASSWORD) {
    if (-not $Iloktool) { Fail "wraptool found but iloktool.exe not found - is License Support installed?" }
    try {
        & $Iloktool cloud --open --account $PaceAccount --password $env:PACE_PASSWORD -v
        if ($LASTEXITCODE -ne 0) { Fail "iloktool cloud --open failed" }
        $cloudSession = $true
        Get-ChildItem -Path (Join-Path $Stage "AAX") -Filter "*.aaxplugin" | ForEach-Object {
            Write-Host "wraptool signing: $($_.Name)"
            & $Wraptool sign --account $PaceAccount --password $env:PACE_PASSWORD `
                --wcguid $PaceWcguid --in $_.FullName --out $_.FullName --allowsigningservice
            if ($LASTEXITCODE -ne 0) { Fail "wraptool failed on $($_.Name)" }
        }
    } finally {
        if ($cloudSession) { & $Iloktool cloud --close | Out-Null }
    }
} else {
    Write-Warning "PACE tools/credentials not available - packaging UNSIGNED AAX (retail Pro Tools will not load them)"
}

# ---- compile the installer --------------------------------------------------
$Iscc = if ($env:ISCC) { $env:ISCC } else { "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" }
if (-not (Test-Path $Iscc)) { Fail "Inno Setup compiler not found at $Iscc (choco install innosetup)" }

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
& $Iscc "/DVersion=$Version" "/DStageDir=$Stage" "/DRepoRoot=$RepoRoot" `
        "/O$((Resolve-Path $OutDir).Path)" `
        (Join-Path $PSScriptRoot "DataCogsPlugins.iss")
if ($LASTEXITCODE -ne 0) { Fail "ISCC failed" }

Write-Host "Built: $OutDir\DataCogs Plugins-$Version.exe"

} finally {
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $Stage
}
