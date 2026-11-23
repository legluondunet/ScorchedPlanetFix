# Clean-room Matrox MSI95 v0.36 build for Windows x86.
# No proprietary Matrox driver source or binaries are used.
# BenchmarkUvReference makes a *temporary* source copy to compile the old
# u-floor(u) behavior, never included in game/installer artifacts.
param([switch]$BenchmarkUvReference)
$ErrorActionPreference = "Stop"
$sourceRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = (Resolve-Path (Join-Path $sourceRoot "../..")).Path
$build = Join-Path $projectRoot "target/matrox-msi95-x86"
New-Item -ItemType Directory -Force -Path $build | Out-Null

# Use the directly reviewable, hash-locked C source committed to the repo.
$source = (Resolve-Path (Join-Path $sourceRoot "msi95_probe.c")).Path
$expectedSourceHash = "f43f765db9cc7914902abb7484204f9399acb3f4dc77348c65c13b694bcec529"
$sourceHash = (Get-FileHash $source -Algorithm SHA256).Hash.ToLowerInvariant()
if ($sourceHash -cne $expectedSourceHash) { throw "MSI95 source checksum mismatch: $sourceHash" }
if ($BenchmarkUvReference) {
    # One auditable replacement after verifying the pristine production source.
    # This creates a reference renderer with identical C flags/link settings.
    $raw = [IO.File]::ReadAllText($source)
    $needle = '    if(u>=0.0f && u<1.0f)return u;'
    if ([regex]::Matches($raw,[regex]::Escape($needle)).Count -ne 1) {
        throw 'MSI95 UV reference benchmark source anchor mismatch'
    }
    $raw = $raw.Replace($needle,
        '    /* Benchmark reference: execute original floor wrap unconditionally. */')
    $source = Join-Path $build 'msi95_uv_reference.c'
    [IO.File]::WriteAllText($source,$raw,[System.Text.UTF8Encoding]::new($false))
}

foreach ($lib in @("kernel32", "user32", "gdi32")) {
    $def = (Resolve-Path (Join-Path $sourceRoot "$lib.def")).Path
    $out = Join-Path $build "$lib.lib"
    & lld-link /lib /MACHINE:X86 "/DEF:$def" "/OUT:$out"
    if ($LASTEXITCODE -ne 0) { throw "lld-link import library failed: $lib" }
}

$obj = Join-Path $build $(if ($BenchmarkUvReference) { "msi95_uv_reference.obj" } else { "msi95_probe.obj" })
& clang --target=i686-pc-windows-msvc -O2 -ffreestanding -fno-stack-protector -I $sourceRoot -c $source -o $obj
if ($LASTEXITCODE -ne 0) { throw "MSI95 x86 C compilation failed" }

$def = (Resolve-Path (Join-Path $sourceRoot "msi95_probe.def")).Path
$dll = Join-Path $build $(if ($BenchmarkUvReference) { "msi95_uv_reference.dll" } else { "msi95.dll" })
$arguments = @(
    "/DLL", "/MACHINE:X86", "/NODEFAULTLIB", "/ENTRY:DllMain@12",
    "/OUT:$dll", "/DEF:$def", $obj,
    (Join-Path $build "kernel32.lib"),
    (Join-Path $build "user32.lib"),
    (Join-Path $build "gdi32.lib")
)
$apiAliases = @(
    "CreateFileA@28", "GetTickCount@0", "QueryPerformanceCounter@4",
    "QueryPerformanceFrequency@4", "Sleep@4", "GetEnvironmentVariableA@12",
    "GetModuleHandleA@4", "GetProcAddress@8",
    "WriteFile@20", "CloseHandle@4", "GetModuleFileNameA@12",
    "GetFileAttributesA@4",
    "VirtualAlloc@16", "VirtualFree@12", "AddVectoredExceptionHandler@8",
    "VirtualQuery@12", "RegisterClassA@4", "CreateWindowExA@48",
    "ShowWindow@8", "UpdateWindow@4", "LoadImageA@24", "SendMessageA@16",
    "PeekMessageA@20",
    "TranslateMessage@4", "DispatchMessageA@4", "DefWindowProcA@16",
    "CallWindowProcA@20", "GetDC@4", "ReleaseDC@8",
    "GetClientRect@8", "AdjustWindowRect@12", "GetSystemMetrics@4",
    "DestroyWindow@4", "BeginPaint@8", "EndPaint@8",
    "StretchDIBits@52", "PatBlt@24"
)
foreach ($alias in $apiAliases) {
    $arguments += "/alternatename:__imp__$alias=__imp__" + $alias.Split("@")[0]
}
& lld-link @arguments
if ($LASTEXITCODE -ne 0) { throw "MSI95 clean-room DLL linking failed" }

$hash = (Get-FileHash $dll -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Host "MATROX_SOURCE_SHA256=$sourceHash"
Write-Host "MATROX_MSI95_DLL_SHA256=$hash"
Write-Host "MATROX_MSI95_DLL=$dll"
if ($BenchmarkUvReference) {
    Write-Host "MATROX_UV_BENCHMARK_REFERENCE=1; benchmark-only DLL, NEVER package"
}
