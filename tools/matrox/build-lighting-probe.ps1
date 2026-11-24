# SPDX-License-Identifier: GPL-3.0-or-later
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$out = Join-Path (Resolve-Path "$root/../..") 'target/matrox-lighting-probe'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$imports = @{
  kernel32 = @('LoadLibraryA@4','GetProcAddress@8','FreeLibrary@4','CreateFileA@28','WriteFile@20','CloseHandle@4','FlushFileBuffers@4','GetLastError@0','DeleteFileA@4','ExitProcess@4')
  user32 = @('DefWindowProcA@16')
}
$aliases = @()
$libs = @()
foreach ($lib in @('kernel32','user32')) {
    $def = Join-Path $out "$lib.def"
    $libpath = Join-Path $out "$lib.lib"
    $names = $imports[$lib] | ForEach-Object { $_.Split('@')[0] }
    @("LIBRARY $lib.dll",'EXPORTS') + $names | Set-Content -Encoding ascii $def
    & lld-link /lib /MACHINE:X86 "/DEF:$def" "/OUT:$libpath"
    if ($LASTEXITCODE -ne 0) { throw "Probe import library failed: $lib" }
    $libs += $libpath
    foreach ($name in $imports[$lib]) { $aliases += "/alternatename:__imp__$name=__imp__" + $name.Split('@')[0] }
}
$obj = Join-Path $out 'lighting_probe.obj'
$exe = Join-Path $out 'MatroxLightingProbe.exe'
& clang --target=i586-pc-windows-msvc -march=pentium -mtune=pentium -O2 -ffreestanding -fno-stack-protector -mno-mmx -mno-sse -mno-sse2 -c "$root/lighting_probe.c" -o $obj
if ($LASTEXITCODE -ne 0) { throw 'Lighting probe C compile failed' }
& lld-link /MACHINE:X86 /NODEFAULTLIB /ENTRY:probe_main /SUBSYSTEM:CONSOLE,4.0 /OSVERSION:4.0 /DYNAMICBASE:NO /NXCOMPAT:NO "/OUT:$exe" $obj @libs @aliases
if ($LASTEXITCODE -ne 0) { throw 'Lighting probe link failed' }
# Audit linked machine code, not just the compiler flags.
$disassembly = & llvm-objdump -d --x86-asm-syntax=intel $exe
if ($LASTEXITCODE -ne 0) { throw 'Cannot disassemble lighting probe' }
$forbidden = $disassembly | Where-Object { $_ -match '\b(cmov[a-z]+|fcmov[a-z]+|fcomi[p]?|fucomi[p]?|sysenter|sysexit|ud2|nop[wdl]?\s+(?:word|dword|qword)\s+ptr)\b' -or $_ -match '^\s*[0-9a-f]+:\s+(?:(?:66|2e|3e|67)\s+)*0f\s+1f\s' }
if ($forbidden) { throw "Post-Pentium instructions found: $forbidden" }
Write-Host 'MATROX_LIGHTING_PROBE_PENTIUM_ISA_AUDIT_PASS'
Write-Host "MATROX_LIGHTING_PROBE=$exe"
