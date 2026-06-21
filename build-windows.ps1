<#
.SYNOPSIS
  Builds the cc65/Lynx host tools by running MSBuild on compiler\cc65.sln.

.DESCRIPTION
  Locates MSBuild via vswhere and builds the solution. Output binaries land in
  the repo-local bin\ directory (per the .vcxproj OutDir). This script builds
  ONLY the solution; it does not build the Lynx runtime library or examples.

.PARAMETER Configuration
  Solution configuration to build. Default: Release.

.PARAMETER Clean
  Perform a clean build (Rebuild target) instead of incremental Build.
#>
[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Definition
$solution = Join-Path $repoRoot 'compiler\cc65.sln'

if (-not (Test-Path $solution)) {
    throw "Solution not found: $solution"
}

# Locate MSBuild via vswhere.
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
    throw "vswhere.exe not found at $vswhere. Install Visual Studio (with the C++ workload)."
}

$msbuild = & $vswhere -latest -prerelease -products * `
    -requires Microsoft.Component.MSBuild `
    -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1

if (-not $msbuild) {
    throw "MSBuild.exe not found. Install the 'Desktop development with C++' workload."
}

Write-Host "MSBuild:       $msbuild"
Write-Host "Solution:      $solution"
Write-Host "Configuration: $Configuration|Win32"

$target = if ($Clean) { 'Rebuild' } else { 'Build' }

# Retarget the toolset/SDK to whatever this VS install actually provides, so the
# build does not fail on the pinned v141 / 10.0.16299.0 if those are absent.
$msbuildArgs = @(
    $solution,
    "/t:$target",
    "/p:Configuration=$Configuration",
    '/p:Platform=Win32',
    '/p:PlatformToolset=v143',
    '/p:WindowsTargetPlatformVersion=10.0',
    '/m',
    '/nologo',
    '/verbosity:minimal'
)
& $msbuild @msbuildArgs

if ($LASTEXITCODE -ne 0) {
    throw "MSBuild failed with exit code $LASTEXITCODE."
}

Write-Host ""
Write-Host "Build succeeded. Tools in: $(Join-Path $repoRoot 'bin')"
