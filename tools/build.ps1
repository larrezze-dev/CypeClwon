param([ValidateSet("release","debug")][string]$Config = "release", [switch]$NoTest)
# Local CI: configure, build and run ctest inside the MSVC developer environment.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found: $vcvars" }
$preset = "msvc-$Config"
$bat = Join-Path $root "build\_ci.bat"
New-Item -ItemType Directory -Force (Split-Path $bat) | Out-Null
$lines = @("@echo off", "call `"$vcvars`" >nul", "cd /d `"$root`"",
  "set PATH=$root\.toolchain\Qt\6.8.3\msvc2022_64\bin;%PATH%",
  "cmake --preset $preset || exit /b 1", "cmake --build --preset $preset || exit /b 1")
if (-not $NoTest) { $lines += "ctest --preset $preset || exit /b 1" }
Set-Content $bat $lines -Encoding ASCII
& cmd /c $bat
if ($LASTEXITCODE -ne 0) { throw "Build or tests failed (exit $LASTEXITCODE)" }

