# Requires: PowerShell 5+ / 7+ and CMake toolchain installed
# Purpose : Build and run eva test suites (unit/functional) with ctest fallback.
# Notes   : Works from any directory; the script locates repo root via its own path.

[CmdletBinding()]
param(
    # Force configure+build before running tests
    [switch]$Build = $true,

    # Enable and run functional tests (off by default unless already configured)
    [switch]$Functional,

    # Label filter: unit | functional | all
    [ValidateSet('unit','functional','all')]
    [string]$Label = 'all',

    # Optional regex to filter test names (ctest -R)
    [string]$Regex,

    # Parallel jobs for build/ctest; 0 = auto
    [int]$Jobs = 0,

    # Enable coverage build and generate report (requires gcovr toolchain)
    [switch]$Coverage = $true,

    # Extra arguments passed to ctest (array)
    [string[]]$CTestArgs = @(),

    # Multi-config generator build configuration (e.g. Debug/Release). If omitted we auto-select Release for VS/Xcode style generators.
    [string]$Config
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Write-Info($msg){ Write-Host "[INFO] $msg" -ForegroundColor Cyan }
function Write-Warn($msg){ Write-Warning $msg }
function Write-Err ($msg){ Write-Host "[ERROR] $msg" -ForegroundColor Red }

function Sync-CMakeBuildConfig {
    param(
        [string]$CachePath,
        [bool]$ConfigProvided,
        [ref]$IsMultiConfig,
        [ref]$ShouldPassConfig,
        [ref]$ConfigValue
    )

    if (-not (Test-Path $CachePath)) { return }

    $multi = $false
    $generator = $null
    $configTypes = $null
    foreach ($line in Get-Content $CachePath) {
        if ($line -match '^CMAKE_GENERATOR:INTERNAL=(.+)$') {
            $generator = $matches[1]
        }
        elseif ($line -match '^CMAKE_CONFIGURATION_TYPES:STRING=(.+)$') {
            $configTypes = $matches[1]
            $multi = $true
        }
    }

    $IsMultiConfig.Value = $multi

    $currentConfig = $ConfigValue.Value
    if ($null -ne $currentConfig) {
        $currentConfig = $currentConfig.Trim()
        if ($currentConfig.Length -eq 0) {
            $currentConfig = $null
        }
    }

    if ($multi -and -not $ConfigProvided -and -not $currentConfig) {
        # Default to Release config for multi-config generators
        $currentConfig = 'Release'
        Write-Info "Multi-configuration generator detected ($generator). Using configuration '$currentConfig'."
    }

    $ConfigValue.Value = $currentConfig
    $ShouldPassConfig.Value = [bool]$currentConfig
}

$scriptDir = Split-Path -Parent $PSCommandPath
$repoRoot  = Split-Path -Parent $scriptDir
Push-Location $repoRoot

try {
    if ($Jobs -le 0) {
        $Jobs = [Math]::Max(2, [int]$env:NUMBER_OF_PROCESSORS)
    }

    $buildDir = Join-Path $repoRoot 'build'
    $binDir   = Join-Path $buildDir 'bin'
    $configProvided = $PSBoundParameters.ContainsKey('Config') -and -not [string]::IsNullOrWhiteSpace($Config)
    if ($configProvided) {
        $Config = $Config.Trim()
        if ($Config.Length -eq 0) {
            $Config = $null
            $configProvided = $false
        }
    }
    $isMultiConfig = $false
    $shouldPassConfig = $false

    # Configure/build when requested or when build/bin is missing
    $needsBuild = $Build -or -not (Test-Path $binDir)
    if ($needsBuild) {
        Write-Info "Configuring CMake (Functional=$Functional, Coverage=$Coverage)..."
        $cmakeArgs = @('-S','.','-B','build','-DEVA_ENABLE_UNIT_TESTS=ON')

        $selectedGenerator = $null
        if (-not [string]::IsNullOrWhiteSpace($env:CMAKE_GENERATOR)) {
            Write-Info "Honoring existing CMAKE_GENERATOR=$($env:CMAKE_GENERATOR)"
        }
        elseif ($IsWindows -and $Coverage) {
            $gxx    = Get-Command g++ -ErrorAction SilentlyContinue
            $mingw  = Get-Command mingw32-make -ErrorAction SilentlyContinue
            if ($gxx -and $mingw) {
                $selectedGenerator = 'MinGW Makefiles'
                Write-Info "Coverage requested on Windows: selecting generator '$selectedGenerator'"
            }
            else {
                Write-Warn "Coverage requested but MinGW toolchain not found; falling back to default generator (coverage will be skipped)."
                $Coverage = $false
            }
        }
        elseif ($IsWindows) {
            $gxx   = Get-Command g++ -ErrorAction SilentlyContinue
            $mingw = Get-Command mingw32-make -ErrorAction SilentlyContinue
            if ($gxx -and $mingw) {
                $selectedGenerator = 'MinGW Makefiles'
                Write-Info "Selecting generator '$selectedGenerator' for MinGW toolchain."
            }
        }
        if ($selectedGenerator) {
            $cmakeArgs = @('-G', $selectedGenerator) + $cmakeArgs
        }

        if ($Functional) { $cmakeArgs += '-DEVA_ENABLE_FUNCTIONAL_TESTS=ON' }
        if ($Coverage)   { $cmakeArgs += '-DEVA_ENABLE_COVERAGE=ON' }

        $isVerbose = $PSBoundParameters.ContainsKey('Verbose') -or $VerbosePreference -eq 'Continue'
        if ($isVerbose) { Write-Info "cmake $($cmakeArgs -join ' ')" }
        & cmake @cmakeArgs | Write-Host

        $cachePath = Join-Path $buildDir 'CMakeCache.txt'
        Sync-CMakeBuildConfig -CachePath $cachePath -ConfigProvided:$configProvided -IsMultiConfig ([ref]$isMultiConfig) -ShouldPassConfig ([ref]$shouldPassConfig) -ConfigValue ([ref]$Config)

        Write-Info "Building (jobs=$Jobs)..."
        $buildArgs = @('--build','build','-j',"$Jobs")
        if ($shouldPassConfig) { $buildArgs += @('--config', $Config) }
        if ($isVerbose) { Write-Info "cmake $($buildArgs -join ' ')" }
        & cmake @buildArgs | Write-Host
    } else {
        Write-Info "Using existing build at $buildDir"
        $cachePath = Join-Path $buildDir 'CMakeCache.txt'
        Sync-CMakeBuildConfig -CachePath $cachePath -ConfigProvided:$configProvided -IsMultiConfig ([ref]$isMultiConfig) -ShouldPassConfig ([ref]$shouldPassConfig) -ConfigValue ([ref]$Config)
    }

    # Try ctest first
    $ctest = Get-Command ctest -ErrorAction SilentlyContinue
    if ($ctest) {
        $args = @('--test-dir', 'build', '--output-on-failure', '-j', "$Jobs")
        if ($Label -and $Label -ne 'all') { $args += @('-L', $Label) }
        if ($Regex)                       { $args += @('-R', $Regex) }
        if ($shouldPassConfig)             { $args += @('-C', $Config) }
        if ($CTestArgs.Count -gt 0)       { $args += $CTestArgs }

        Write-Info "Running tests via ctest..."
        $isVerbose = $PSBoundParameters.ContainsKey('Verbose') -or $VerbosePreference -eq 'Continue'
        if ($isVerbose) { Write-Info "ctest $($args -join ' ')" }
        & $ctest.Source @args
        $code = $LASTEXITCODE

        if ($code -ne 0) { throw "ctest reported failures (exit $code)" }
    }
    else {
        # Fallback: discover and run test executables directly
        Write-Warn "ctest not found; falling back to direct execution."

        $candidateDirs = New-Object System.Collections.Generic.List[string]
        if ($shouldPassConfig) {
            $configBin = Join-Path $binDir $Config
            if (Test-Path $configBin) { $candidateDirs.Add($configBin) }
        }
        if (Test-Path $binDir) { $candidateDirs.Add($binDir) }
        $uniqueDirs = $candidateDirs | Select-Object -Unique

        if ($uniqueDirs.Count -eq 0) {
            throw "Test binaries not found: $binDir"
        }

        $patterns = @('*tests.exe','*tests')  # Windows + POSIX
        $allTests = @()
        foreach ($dir in $uniqueDirs) {
            if (Test-Path $dir) {
                $allTests += Get-ChildItem -Path $dir -File -Recurse -Include $patterns
            }
        }
        $allTests = $allTests | Sort-Object Name
        if ($allTests.Count -eq 0) { throw "No test executables found in $binDir" }

        # Label filtering approximation for fallback mode
        $tests = $allTests
        switch ($Label) {
            'unit'       { $tests = $allTests | Where-Object { $_.BaseName -ne 'eva_functional_tests' } }
            'functional' { $tests = $allTests | Where-Object { $_.BaseName -eq 'eva_functional_tests' } }
            default { }
        }
        if ($Regex) {
            $tests = $tests | Where-Object { $_.BaseName -match $Regex }
        }

        if ($tests.Count -eq 0) {
            Write-Warn "No tests matched Label='$Label' Regex='$Regex'"
            exit 0
        }

        $fail = 0
        foreach ($t in $tests) {
            Write-Host "==== RUN $($t.Name) ====" -ForegroundColor Yellow
            # Run without extra args for maximum compatibility
            & $t.FullName
            $exit = $LASTEXITCODE
            if ($exit -ne 0) {
                Write-Err "FAILED: $($t.Name) (exit $exit)"
                $fail++
            } else {
                Write-Info "OK: $($t.Name)"
            }
        }

        if ($fail -gt 0) {
            throw "Some tests failed: $fail"
        }
    }

    if ($Coverage) {
        Write-Info "Generating coverage report (requires gcovr)..."
        try {
            $coverageArgs = @('--build','build','--target','coverage')
            if ($shouldPassConfig) { $coverageArgs += @('--config', $Config) }
            & cmake @coverageArgs | Write-Host
        } catch {
            Write-Warn "Coverage target failed or gcovr missing. Skip."
        }
    }

    Write-Info "All tests passed."
    exit 0
}
catch {
    Write-Err $_
    exit 1
}
finally {
    Pop-Location
}
