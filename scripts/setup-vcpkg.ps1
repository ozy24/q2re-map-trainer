# One-time vcpkg setup for q2re-map-trainer.
# Initializes the vcpkg submodule, bootstraps the tool, and installs manifest dependencies.

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
if (-not (Test-Path (Join-Path $Root "src\vcpkg.json"))) {
    Write-Error "Could not locate repo root (expected src\vcpkg.json)."
}

$VcpkgDir = Join-Path $Root "vcpkg"
$VcpkgExe = Join-Path $VcpkgDir "vcpkg.exe"
$SrcDir = Join-Path $Root "src"

Write-Host "[vcpkg] Repo root: $Root"

Push-Location $Root
try {
    if (-not (Test-Path (Join-Path $VcpkgDir ".git"))) {
        Write-Host "[vcpkg] Initializing submodule..."
        git submodule update --init --recursive vcpkg
        if ($LASTEXITCODE -ne 0) { throw "git submodule update failed." }
    }

    if (-not (Test-Path $VcpkgExe)) {
        Write-Host "[vcpkg] Bootstrapping vcpkg..."
        Push-Location $VcpkgDir
        try {
            & .\bootstrap-vcpkg.bat -disableMetrics
            if ($LASTEXITCODE -ne 0) { throw "bootstrap-vcpkg.bat failed." }
        } finally {
            Pop-Location
        }
    }

    Write-Host "[vcpkg] Installing manifest dependencies (x64-windows-static)..."
    $env:VCPKG_ROOT = $VcpkgDir
    Push-Location $SrcDir
    try {
        & $VcpkgExe install --triplet x64-windows-static
        if ($LASTEXITCODE -ne 0) { throw "vcpkg install failed." }
    } finally {
        Pop-Location
    }

    Write-Host "[vcpkg] Setup complete."
} finally {
    Pop-Location
}
