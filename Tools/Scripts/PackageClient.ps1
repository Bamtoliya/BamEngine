param(
    [string]$Config = "Debug",
    [string]$OutputDir = "out/package/Client"
)

$ErrorActionPreference = "Stop"

$Root = Resolve-Path "$PSScriptRoot/../.."
$BinDir = "$Root/out/build/x64-$($Config.ToLower())/bin"
if (-not (Test-Path $BinDir)) {
    $BinDir = "$Root/Bin/$Config"
}

Write-Host "=== Packaging BamEngine Client ($Config) ===" -ForegroundColor Cyan
Write-Host "Source Bin: $BinDir"
Write-Host "Target Dir: $OutputDir"

# 1. 대상 폴더 생성
if (Test-Path $OutputDir) {
    Remove-Item -Recurse -Force $OutputDir
}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
New-Item -ItemType Directory -Path "$OutputDir/Logs" -Force | Out-Null

# 2. 실행 바이너리 및 필수 DLL 복사
$BinFiles = @("Client.exe", "Engine.dll", "GameModule.dll", "SDL3.dll")
if ($Config -eq "Debug") {
    $BinFiles += "fmodL.dll"
} else {
    $BinFiles += "fmod.dll"
}

foreach ($file in $BinFiles) {
    $src = Join-Path $BinDir $file
    if (Test-Path $src) {
        Copy-Item -Path $src -Destination $OutputDir -Force
        Write-Host "Copied: $file" -ForegroundColor Green
    } else {
        Write-Warning "Missing optional/required binary: $src"
    }
}

# 3. 런타임 리소스 복사 (Stage)
$ResourcesToCopy = @(
    "Data",
    "Material",
    "Scenes",
    "Shader",
    "Texture",
    "Fonts",
    "Audio"
)

$TargetRes = Join-Path $OutputDir "Resources"
New-Item -ItemType Directory -Path $TargetRes -Force | Out-Null

foreach ($folder in $ResourcesToCopy) {
    $srcFolder = Join-Path $Root "Resources/$folder"
    if (Test-Path $srcFolder) {
        Copy-Item -Path $srcFolder -Destination $TargetRes -Recurse -Force
        Write-Host "Copied Resource folder: $folder" -ForegroundColor Green
    }
}

if (Test-Path "$Root/Resources/icon.bmp") {
    Copy-Item -Path "$Root/Resources/icon.bmp" -Destination $TargetRes -Force
}

Write-Host "`nPackaging completed successfully at: $OutputDir" -ForegroundColor Cyan