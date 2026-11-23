# SPDX-License-Identifier: GPL-3.0-or-later
# Read-only, game-data-free x86 MSI95 in-range UV versus original floor ABBA.
# Run only in CI/test jobs; reference DLL must NEVER be released/embedded.
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Push-Location $projectRoot
try {
    # The original hash-locked C source is unchanged. Build a second DLL
    # from a temporary one-line reference variant, same x86 -O2 flags.
    & "$PSScriptRoot/build-msi95.ps1" -BenchmarkUvReference
    if (-not (Test-Path -LiteralPath 'target/matrox-msi95-x86/msi95_uv_reference.dll')) {
        throw 'Could not build matched reference-floor MSI95'
    }
    $folder = Join-Path $env:RUNNER_TEMP 'scorched-msi95-uv-benchmark'
    New-Item -ItemType Directory -Path $folder -Force | Out-Null
    Copy-Item 'target/matrox-msi95-x86/msi95.dll' (Join-Path $folder 'msi95.dll')
    Copy-Item 'target/matrox-msi95-x86/msi95_uv_reference.dll' (Join-Path $folder 'msi95_uv_reference.dll')
    $exe = Join-Path $folder 'MatroxUvBench.exe'
    rustc --edition=2021 -O --target i686-pc-windows-msvc tools/matrox/bench_uv.rs -o $exe
    if ($LASTEXITCODE -ne 0) { throw 'Cannot compile x86 real-raster UV benchmark' }
    $records = @()
    Push-Location $folder
    try {
        # ABBA reduces warmup and clock drift. Each invocation executes
        # identical synthetic geometry/CLUT with native x86 MSI95 DLL.
        foreach ($mode in @('reference','optimized','optimized','reference')) {
            $dll = if ($mode -eq 'reference') {'msi95_uv_reference.dll'} else {'msi95.dll'}
            $lines = @(& $exe $dll)
            if ($LASTEXITCODE -ne 0) { throw "UV x86 real-raster benchmark failed: $mode" }
            foreach ($line in $lines) {
                Write-Host "$mode $line"
                if ($line -match '^BENCH_UV profile=(\w+) render_us=(\d+) checksum=([0-9a-f]{16}) triangles=(\d+)$') {
                    $records += [PSCustomObject]@{
                        Mode=$mode; Profile=$Matches[1]; Us=[long]$Matches[2];
                        Hash=$Matches[3]; Triangles=[int]$Matches[4]
                    }
                }
            }
        }
    } finally {
        Pop-Location
    }
    if ($records.Count -ne 8) {
        throw "Expected eight ABBA profile measurements, got $($records.Count)"
    }
    foreach ($profile in @('inrange','tiled')) {
        $rows = @($records | Where-Object Profile -eq $profile)
        $hashes = @($rows | Select-Object -ExpandProperty Hash -Unique)
        if ($rows.Count -ne 4 -or $hashes.Count -ne 1) {
            throw "UV baseline versus optimized real pixel output mismatch: $profile"
        }
        if (@($rows | Where-Object Triangles -ne 1680).Count -ne 0) {
            throw "UV benchmark workload mismatch: $profile"
        }
        $baseline = [double](($rows | Where-Object Mode -eq 'reference' |
            Measure-Object -Property Us -Average).Average)
        $optimized = [double](($rows | Where-Object Mode -eq 'optimized' |
            Measure-Object -Property Us -Average).Average)
        if ($baseline -le 0 -or $optimized -le 0) {
            throw "Invalid UV timings: $profile"
        }
        $change = 100.0 * ($optimized / $baseline - 1.0)
        Write-Host ("UV_ABBA_RESULT profile={0} baseline_us={1:N0} optimized_us={2:N0} delta_pct={3:F2} pixel_hash={4} triangles_per_run=1680" -f $profile,$baseline,$optimized,$change,$hashes[0])
    }
    # Shared runner timings are observational; parity, not speed, gates CI.
} finally {
    Pop-Location
}