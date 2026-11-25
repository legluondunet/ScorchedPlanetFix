# Clean-room Matrox MSI95 v0.36 build for Windows x86.
# No proprietary Matrox driver source or binaries are used.
$ErrorActionPreference = "Stop"
$sourceRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = (Resolve-Path (Join-Path $sourceRoot "../..")).Path
$build = Join-Path $projectRoot "target/matrox-msi95-x86"
New-Item -ItemType Directory -Force -Path $build | Out-Null

# Use the directly reviewable, hash-locked C source committed to the repo.
$source = (Resolve-Path (Join-Path $sourceRoot "msi95_probe.c")).Path
$expectedSourceHash = "f1186149267866364f356e8641fedc9e9c6f03d36381094727a2d83b8f22017f"
$sourceHash = (Get-FileHash $source -Algorithm SHA256).Hash.ToLowerInvariant()
if ($sourceHash -cne $expectedSourceHash) { throw "MSI95 source checksum mismatch: $sourceHash" }
$waveHeaderHash = (Get-FileHash (Join-Path $sourceRoot "msi_gpu_batch.h") -Algorithm SHA256).Hash.ToLowerInvariant()
if ($waveHeaderHash -cne "521478141d660739d7f29fba2d8e8c5bb52201d42ef3a2bc56f89d113164f1d9") { throw "GPU wave header checksum mismatch: $waveHeaderHash" }
if ((Get-FileHash (Join-Path $sourceRoot "msi_gpu_draw_batch.h") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "dbfd7c5ddea03fb440b2392ee12426c1d565f716b785620053bad6157eef980a") { throw "GPU header checksum mismatch: msi_gpu_draw_batch.h" }
if ((Get-FileHash (Join-Path $sourceRoot "msi_gpu_snapshot.h") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "300028ebbc25f278eb063b65c82901904b869c595da477a2a89f7930fdb639cf") { throw "GPU header checksum mismatch: msi_gpu_snapshot.h" }
if ((Get-FileHash (Join-Path $sourceRoot "msi_gpu_texel.h") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "e51a6794beeb7a8cfa361ffdeba2e4ab1d08a2e2af72f42f16f832f5c81f2264") { throw "GPU header checksum mismatch: msi_gpu_texel.h" }

foreach ($lib in @("kernel32", "user32", "gdi32")) {
    $def = (Resolve-Path (Join-Path $sourceRoot "$lib.def")).Path
    $out = Join-Path $build "$lib.lib"
    & lld-link /lib /MACHINE:X86 "/DEF:$def" "/OUT:$out"
    if ($LASTEXITCODE -ne 0) { throw "lld-link import library failed: $lib" }
}

$obj = Join-Path $build "msi95_probe.obj"
& clang --target=i686-pc-windows-msvc -O2 -ffreestanding -fno-stack-protector -c $source -o $obj
if ($LASTEXITCODE -ne 0) { throw "MSI95 x86 C compilation failed" }

$vkSource = Join-Path $sourceRoot "vulkan_present.c"
$vkHeader = Join-Path $sourceRoot "vulkan_present.h"
$vkSourceHash = (Get-FileHash $vkSource -Algorithm SHA256).Hash.ToLowerInvariant()
if ($vkSourceHash -cne "87ca710593c0510cb923ca940cfba5ba94c83ad228108d69006a338d9586215e") { throw "Vulkan presenter source checksum mismatch: $vkSourceHash" }
$vkHeaderHash = (Get-FileHash $vkHeader -Algorithm SHA256).Hash.ToLowerInvariant()
if ($vkHeaderHash -cne "9d1fdc68630dcc0f9cb1955fd4539b81670db1afb9625a981da9ecb024ac37d6") { throw "Vulkan presenter header checksum mismatch: $vkHeaderHash" }
if ((Get-FileHash (Join-Path $sourceRoot "msi_vk_batch_replay.h") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "db846b6d0bc4894b33db9d7d7eaa8218e68ff43b8380ca964631e953bc82194e") { throw "Shadow source checksum mismatch: msi_vk_batch_replay.h" }
if ((Get-FileHash (Join-Path $sourceRoot "msi_vk_shadow_session.h") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "06c50cc3b2304ce9bca3425d3e55c226b0b868fabdaaeb08cf6a5fb0326dacf7") { throw "Shadow source checksum mismatch: msi_vk_shadow_session.h" }
if ((Get-FileHash (Join-Path $sourceRoot "msi_vk_shadow_bridge.c") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "435b91139c089606135709883e484fc766a3cc624f5527ea682caa9cddb17f10") { throw "Shadow source checksum mismatch: msi_vk_shadow_bridge.c" }
if ((Get-FileHash (Join-Path $sourceRoot "msi_vk_shadow_bridge.h") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "b84069b47bc9ff81941e456c382d649a530f815cfec72a43adb92283fed1c3ee") { throw "Shadow source checksum mismatch: msi_vk_shadow_bridge.h" }
if ((Get-FileHash (Join-Path $sourceRoot "msi_gpu_compare.h") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "202b14bbe97a6a30154113dbf12f83a738e19c8a43fa3b6b79cb843c18eeaf72") { throw "Shadow source checksum mismatch: msi_gpu_compare.h" }
if ((Get-FileHash (Join-Path $sourceRoot "world_spv_embedded.h") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "cf822014d61d7d25d0d78f13acb0dd7b38ddad9317288f832667a3c2ee46a732") { throw "Shadow source checksum mismatch: world_spv_embedded.h" }
if ((Get-FileHash (Join-Path $sourceRoot "shaders/opaque_world.vert") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "279dd28f45eeab662b6f5ef663b90668b9518085e1dfff711552839a719339c5") { throw "Shadow source checksum mismatch: shaders/opaque_world.vert" }
if ((Get-FileHash (Join-Path $sourceRoot "shaders/opaque_world.frag") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "eca7a42521fcf7ab765a7ecd99a627b18c64a30f6b64412120f695d8a875230e") { throw "Shadow source checksum mismatch: shaders/opaque_world.frag" }
if ((Get-FileHash (Join-Path $sourceRoot "msi_gpu_shadow_sampler.h") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "27b4c493b33f3283466ada32e516f1f5ee5fc808c763fb5666f6cbecc76e4dd9") { throw "Shadow source checksum mismatch: msi_gpu_shadow_sampler.h" }
if ((Get-FileHash (Join-Path $sourceRoot "msi_gpu_publish.h") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "b0046243701d3a62e661bf785ad619efed429f20457fe340de903ee3ff176fd0") { throw "Shadow source checksum mismatch: msi_gpu_publish.h" }
if ((Get-FileHash (Join-Path $sourceRoot "msi_gpu_coverage.h") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "29974e1783d1ef9d74bb88268f3343d21c6a6f43119c35d52917c462d82c274f") { throw "Shadow source checksum mismatch: msi_gpu_coverage.h" }
if ((Get-FileHash (Join-Path $sourceRoot "msi_gpu_bypass.h") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "1369fb274b666e7209c6c28547b72a6177ac2d3d9ccfb7df91d6709dcc640105") { throw "Shadow source checksum mismatch: msi_gpu_bypass.h" }
$vkRoot = Join-Path $projectRoot "third_party/vulkan-headers"
if (!(Test-Path (Join-Path $vkRoot "include/vulkan/vulkan.h"))) { throw "Pinned Vulkan-Headers checkout required" }
$vkCommit = (& git -C $vkRoot rev-parse HEAD).Trim()
if ($vkCommit -cne "b379292b2ab6df5771ba9870d53cf8b2c9295daf") { throw "Unexpected Vulkan-Headers commit: $vkCommit" }
$vkObj = Join-Path $build "vulkan_present.obj"
# clang locates the installed Windows SDK/MSVC headers, no Vulkan import library.
& clang --target=i686-pc-windows-msvc -O2 -ffreestanding -fno-builtin -fno-stack-protector -DMATROX_VK_FREESTANDING -DMATROX_VK_SHADOW "-I$vkRoot/include" -c $vkSource -o $vkObj
if ($LASTEXITCODE -ne 0) { throw "Vulkan presenter x86 compilation failed" }
$shadowObj = Join-Path $build "msi_vk_shadow_bridge.obj"
& clang --target=i686-pc-windows-msvc -O2 -ffreestanding -fno-builtin -fno-stack-protector "-I$vkRoot/include" -c (Join-Path $sourceRoot "msi_vk_shadow_bridge.c") -o $shadowObj
if ($LASTEXITCODE -ne 0) { throw "Vulkan shadow x86 compilation failed" }

$def = (Resolve-Path (Join-Path $sourceRoot "msi95_probe.def")).Path
$dll = Join-Path $build "msi95.dll"
$arguments = @(
    "/DLL", "/MACHINE:X86", "/NODEFAULTLIB", "/ENTRY:DllMain@12",
    "/OUT:$dll", "/DEF:$def", $obj, $vkObj, $shadowObj,
    (Join-Path $build "kernel32.lib"),
    (Join-Path $build "user32.lib"),
    (Join-Path $build "gdi32.lib")
)
$apiAliases = @(
    "CreateFileA@28", "GetTickCount@0", "QueryPerformanceCounter@4",
    "QueryPerformanceFrequency@4", "Sleep@4", "GetEnvironmentVariableA@12",
    "GetModuleHandleA@4", "GetProcAddress@8", "LoadLibraryExW@12", "FreeLibrary@4", "GetModuleHandleW@4", "IsWindow@4", "IsIconic@4",
    "WriteFile@20", "CloseHandle@4", "GetModuleFileNameA@12",
    "GetFileAttributesA@4", "GetLastError@0",
    "VirtualAlloc@16", "VirtualFree@12", "AddVectoredExceptionHandler@8",
    "VirtualQuery@12", "RegisterClassA@4", "CreateWindowExA@48",
    "ShowWindow@8", "UpdateWindow@4", "LoadImageA@24", "SendMessageA@16",
    "PeekMessageA@20",
    "TranslateMessage@4", "DispatchMessageA@4", "DefWindowProcA@16",
    "CallWindowProcA@20", "GetDC@4", "ReleaseDC@8",
    "GetClientRect@8", "AdjustWindowRect@12", "GetSystemMetrics@4",
    "DestroyWindow@4", "BeginPaint@8", "EndPaint@8",
    "StretchDIBits@52", "PatBlt@24", "SetStretchBltMode@8", "SetBrushOrgEx@16"
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
