<#
.SYNOPSIS
    BamEngine Client 로컬 개발용 CLI 스테이징 보조 스크립트 (Legacy / CLI Helper).
.DESCRIPTION
    공식 Windows 배포 패키징 경로는 Editor 내장 패키저(Editor/Source/Build/Private/ClientPackage.cpp)입니다.
    에디터 패키저는 시작 매니페스트(StartupResourceLoader), RenderPipelineProfile, 씬 에셋 참조 검증을 수행합니다.
    이 스크립트는 빌드된 바이너리와 Engine/Resources, Assets, Config/Project.json을
    저장소 내부 out/package/<하위폴더>에 빠르게 스테이징할 때만 보조용으로 사용합니다.
#>
param(
    [string]$Config = "Debug",
    [string]$OutputDir = "out/package/Client"
)

$ErrorActionPreference = "Stop"

$Root = [System.IO.Path]::GetFullPath((Resolve-Path "$PSScriptRoot/../..").Path).TrimEnd('\', '/')
$AllowedPackageRoot = [System.IO.Path]::Combine($Root, "out", "package")

# ── 1. 출력 경로 절대 정규화 및 파괴적 삭제 방어 (F03 / A01 / B05) ──
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
$ForbiddenDirectories = @(
    "Engine", "Editor", "Client", "Assets", "Resources", "Config",
    "Tools", "Projects", "Docs", "ThirdParty", "Utility", ".git", "Bin", "bin"
)
foreach ($forbidden in $ForbiddenDirectories) {
    $forbiddenPath = [System.IO.Path]::Combine($Root, $forbidden)
    if ($ResolvedOutput -eq $forbiddenPath -or
        $ResolvedOutput.StartsWith("$forbiddenPath\", [System.StringComparison]::OrdinalIgnoreCase) -or
        $ResolvedOutput.StartsWith("$forbiddenPath/", [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Security Violation: OutputDir cannot be inside core project directory: '$forbiddenPath'"
    }
}

# [가드 4] 저장소 외부 경로 및 전용 패키지 루트(out/package/<SubDir>) 외 경로 차단
if (-not $ResolvedOutput.StartsWith("$Root\", [System.StringComparison]::OrdinalIgnoreCase) -and
    -not $ResolvedOutput.StartsWith("$Root/", [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Security Violation: OutputDir must be located inside the repository: '$ResolvedOutput'"
}
if ($ResolvedOutput -eq $AllowedPackageRoot -or
    (-not $ResolvedOutput.StartsWith("$AllowedPackageRoot\", [System.StringComparison]::OrdinalIgnoreCase) -and
     -not $ResolvedOutput.StartsWith("$AllowedPackageRoot/", [System.StringComparison]::OrdinalIgnoreCase))) {
    throw "Security Violation: OutputDir must be a subdirectory under '$AllowedPackageRoot': '$ResolvedOutput'"
}

# [가드 5] 기존 대상이 리파스 포인트(Junction/Symlink)이거나 비패키지 폴더인 경우 거부
if (Test-Path -LiteralPath $ResolvedOutput) {
    $existingItem = Get-Item -LiteralPath $ResolvedOutput -Force
    if (($existingItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Security Violation: OutputDir cannot be a reparse point (junction/symlink): '$ResolvedOutput'"
    }
    $hasMarker = Test-Path -LiteralPath (Join-Path $ResolvedOutput ".bam-client-package")
    $hasClientExe = Test-Path -LiteralPath (Join-Path $ResolvedOutput "Client.exe")
    $hasEntries = (Get-ChildItem -LiteralPath $ResolvedOutput -Force | Select-Object -First 1) -ne $null
    if ($hasEntries -and -not $hasMarker -and -not $hasClientExe) {
        throw "Security Violation: Existing non-empty OutputDir is not a BamEngine Client package: '$ResolvedOutput'"
    }
}

# ── 2. 소스 바이너리 및 필수 입력 사전 검증 (F04 / A02) ──
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
        $BinDir = [System.IO.Path]::GetFullPath($candidate).TrimEnd('\', '/')
        break
    }
}

if (-not $BinDir) {
    throw "Packaging Failure: Could not locate 'Client.exe' for configuration '$Config'. Searched in:`n$($BinDirCandidates -join "`n")"
}
if ($ResolvedOutput.Equals($BinDir, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Security Violation: OutputDir cannot be the source binary directory: '$BinDir'"
}

$RequiredBinaries = @("Client.exe", "Engine.dll", "GameModule.dll", "SDL3.dll")
$OptionalBinaries = @(
    "GameplayScore.dll",
    "fmt.dll",
    "lz4.dll",
    $(if ($Config -eq "Debug") { "fmodL.dll" } else { "fmod.dll" })
)

foreach ($file in $RequiredBinaries) {
    $src = Join-Path $BinDir $file
    if (-not (Test-Path -LiteralPath $src)) {
        throw "Packaging Failure: Missing required binary: $src"
    }
}

$ProjectJsonPath = Join-Path $Root "Config/Project.json"
if (-not (Test-Path -LiteralPath $ProjectJsonPath)) {
    throw "Packaging Failure: Missing essential project config: $ProjectJsonPath"
}

$EngineResourcesRoot = Join-Path $Root "Engine/Resources"
if (-not (Test-Path -LiteralPath $EngineResourcesRoot)) {
    throw "Packaging Failure: Missing Engine/Resources directory: $EngineResourcesRoot"
}

Write-Warning "Note: Official Windows Client packaging with scene/shader/profile validation is provided by Editor Build & Package (ClientPackage.cpp)."
Write-Host "=== Staging BamEngine Client ($Config) [CLI Helper] ===" -ForegroundColor Cyan
Write-Host "Source Bin: $BinDir"
Write-Host "Target Dir: $ResolvedOutput"

# ── 3. 스테이징 디렉터리 생성 및 파일 수집 (A02: 기존 패키지 보존 후 교체) ──
$StagingDir = "$ResolvedOutput.__staging"
if (Test-Path -LiteralPath $StagingDir) {
    Remove-Item -LiteralPath $StagingDir -Recurse -Force
}

[void][System.IO.Directory]::CreateDirectory($StagingDir)
[void][System.IO.Directory]::CreateDirectory((Join-Path $StagingDir "Logs"))
[void][System.IO.Directory]::CreateDirectory((Join-Path $StagingDir "Config"))
[void][System.IO.Directory]::CreateDirectory((Join-Path $StagingDir "Engine"))

# 패키지 소유 마커 생성 (.bam-client-package)
[System.IO.File]::WriteAllText((Join-Path $StagingDir ".bam-client-package"), "BamEngine Client Package (CLI Helper)`n")

foreach ($file in $RequiredBinaries) {
    Copy-Item -LiteralPath (Join-Path $BinDir $file) -Destination $StagingDir -Force
    Write-Host "Copied required binary: $file" -ForegroundColor Green
}

foreach ($file in $OptionalBinaries) {
    $src = Join-Path $BinDir $file
    if (Test-Path -LiteralPath $src) {
        Copy-Item -LiteralPath $src -Destination $StagingDir -Force
        Write-Host "Copied optional binary: $file" -ForegroundColor Green
    } else {
        Write-Warning "Missing optional binary: $src"
    }
}

Copy-Item -LiteralPath $ProjectJsonPath -Destination (Join-Path $StagingDir "Config") -Force
Write-Host "Copied Config: Project.json" -ForegroundColor Green

Copy-Item -LiteralPath $EngineResourcesRoot -Destination (Join-Path $StagingDir "Engine/Resources") -Recurse -Force
Write-Host "Copied Engine/Resources -> Engine/Resources" -ForegroundColor Green

$AssetsRoot = Join-Path $Root "Assets"
if (Test-Path -LiteralPath $AssetsRoot) {
    Copy-Item -LiteralPath $AssetsRoot -Destination (Join-Path $StagingDir "Assets") -Recurse -Force
    Write-Host "Copied Assets -> Assets" -ForegroundColor Green
}

# ── 4. 스테이징 완료 후 대상 디렉터리 교체 ──
if (Test-Path -LiteralPath $ResolvedOutput) {
    Remove-Item -LiteralPath $ResolvedOutput -Recurse -Force
}
Move-Item -LiteralPath $StagingDir -Destination $ResolvedOutput

Write-Host "`nPackaging completed successfully at: $ResolvedOutput" -ForegroundColor Cyan