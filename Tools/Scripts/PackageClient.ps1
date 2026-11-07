param(
    [string]$Config = "Debug",
    [string]$OutputDir = "out/package/Client"
)

$ErrorActionPreference = "Stop"

$Root = [System.IO.Path]::GetFullPath((Resolve-Path "$PSScriptRoot/../..").Path).TrimEnd('\', '/')

# ── 1. 출력 경로 절대 정규화 및 파괴적 삭제 방어 (F03) ──
function Get-NormalizedAbsolutePath([string]$path, [string]$baseDir) {
    if ([System.IO.Path]::IsPathRooted($path)) {
        return [System.IO.Path]::GetFullPath($path).TrimEnd('\', '/')
    } else {
        return [System.IO.Path]::GetFullPath([System.IO.Path]::Combine($baseDir, $path)).TrimEnd('\', '/')
    }
}

$ResolvedOutput = Get-NormalizedAbsolutePath $OutputDir $Root

# [가드 1] 드라이브 루트 삭제 차단 (예: C:\, I:\)
$driveRoot = [System.IO.Path]::GetPathRoot($ResolvedOutput).TrimEnd('\', '/')
if ($ResolvedOutput -eq $driveRoot -or $ResolvedOutput -eq "$driveRoot\") {
    throw "Security Violation: OutputDir cannot be a drive root: '$ResolvedOutput'"
}

# [가드 2] 프로젝트 저장소 루트 및 상위 경로 삭제 차단
if ($ResolvedOutput -eq $Root) {
    throw "Security Violation: OutputDir cannot be the repository root: '$ResolvedOutput'"
}
if ($Root.StartsWith($ResolvedOutput, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Security Violation: OutputDir cannot be a parent directory of the repository: '$ResolvedOutput'"
}

# [가드 3] 프로젝트 핵심 소스/데이터 디렉터리 삭제 차단
$ForbiddenDirectories = @("Engine", "Editor", "Client", "Resources", "Config", "Tools", "Projects", "Docs", ".git", "Bin")
foreach ($forbidden in $ForbiddenDirectories) {
    $forbiddenPath = [System.IO.Path]::Combine($Root, $forbidden)
    if ($ResolvedOutput -eq $forbiddenPath -or $ResolvedOutput.StartsWith("$forbiddenPath\", [System.StringComparison]::OrdinalIgnoreCase) -or $ResolvedOutput.StartsWith("$forbiddenPath/", [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Security Violation: OutputDir cannot be inside core project directory: '$forbiddenPath'"
    }
}

# [가드 4] 프로젝트 저장소 외부의 임의 디렉터리 삭제 방지 (안전하게 프로젝트 하위 디렉터리만 허용)
if (-not $ResolvedOutput.StartsWith("$Root\", [System.StringComparison]::OrdinalIgnoreCase) -and -not $ResolvedOutput.StartsWith("$Root/", [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Security Violation: OutputDir must be located inside the repository: '$ResolvedOutput'"
}

# ── 2. 소스 바이너리 경로 다단계 자동 탐색 (F04) ──
$configLower = $Config.ToLower()
$BinDirCandidates = @(
    "$Root/out/build/x64-$configLower/bin/$Config",
    "$Root/out/build/x64-$configLower/bin",
    "$Root/bin/$Config",
    "$Root/Bin/$Config",
    "$Root/bin",
    "$Root/Bin"
)

$BinDir = $null
foreach ($candidate in $BinDirCandidates) {
    if (Test-Path -LiteralPath (Join-Path $candidate "Client.exe")) {
        $BinDir = $candidate
        break
    }
}

if (-not $BinDir) {
    throw "Packaging Failure: Could not locate 'Client.exe' for configuration '$Config'. Searched in:`n$($BinDirCandidates -join "`n")"
}

Write-Host "=== Packaging BamEngine Client ($Config) ===" -ForegroundColor Cyan
Write-Host "Source Bin: $BinDir"
Write-Host "Target Dir: $ResolvedOutput"

# ── 3. 대상 폴더 안전 초기화 (-LiteralPath 사용) ──
if (Test-Path -LiteralPath $ResolvedOutput) {
    Write-Host "Cleaning existing output directory: $ResolvedOutput" -ForegroundColor Yellow
    Remove-Item -LiteralPath $ResolvedOutput -Recurse -Force
}
New-Item -ItemType Directory -Path $ResolvedOutput -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $ResolvedOutput "Logs") -Force | Out-Null

# ── 4. 실행 바이너리 및 필수 DLL 복사 ──
$RequiredBinaries = @("Client.exe", "Engine.dll", "GameModule.dll", "SDL3.dll")
$OptionalBinaries = if ($Config -eq "Debug") { @("fmodL.dll") } else { @("fmod.dll") }

foreach ($file in $RequiredBinaries) {
    $src = Join-Path $BinDir $file
    if (Test-Path -LiteralPath $src) {
        Copy-Item -LiteralPath $src -Destination $ResolvedOutput -Force
        Write-Host "Copied required binary: $file" -ForegroundColor Green
    } else {
        throw "Packaging Failure: Missing required binary: $src"
    }
}

foreach ($file in $OptionalBinaries) {
    $src = Join-Path $BinDir $file
    if (Test-Path -LiteralPath $src) {
        Copy-Item -LiteralPath $src -Destination $ResolvedOutput -Force
        Write-Host "Copied optional binary: $file" -ForegroundColor Green
    } else {
        Write-Warning "Missing optional binary: $src"
    }
}

# ── 5. 프로젝트 설정 파일 복사 (F04 핵심: Config/Project.json) ──
$TargetConfig = Join-Path $ResolvedOutput "Config"
New-Item -ItemType Directory -Path $TargetConfig -Force | Out-Null

$ProjectJsonPath = Join-Path $Root "Config/Project.json"
if (Test-Path -LiteralPath $ProjectJsonPath) {
    Copy-Item -LiteralPath $ProjectJsonPath -Destination $TargetConfig -Force
    Write-Host "Copied Config: Project.json" -ForegroundColor Green
} else {
    throw "Packaging Failure: Missing essential project config: $ProjectJsonPath"
}

# ── 6. 런타임 리소스 복사 (Stage) ──
$ResourcesToCopy = @(
    "Data",
    "Material",
    "Scenes",
    "Shader",
    "Texture",
    "Fonts",
    "Audio",
    "Model",
    "Config",
    "Scripts"
)

$TargetRes = Join-Path $ResolvedOutput "Resources"
New-Item -ItemType Directory -Path $TargetRes -Force | Out-Null

foreach ($folder in $ResourcesToCopy) {
    $srcFolder = Join-Path $Root "Resources/$folder"
    if (Test-Path -LiteralPath $srcFolder) {
        Copy-Item -LiteralPath $srcFolder -Destination $TargetRes -Recurse -Force
        Write-Host "Copied Resource folder: $folder" -ForegroundColor Green
    }
}

$iconPath = Join-Path $Root "Resources/icon.bmp"
if (Test-Path -LiteralPath $iconPath) {
    Copy-Item -LiteralPath $iconPath -Destination $TargetRes -Force
}

Write-Host "`nPackaging completed successfully at: $ResolvedOutput" -ForegroundColor Cyan