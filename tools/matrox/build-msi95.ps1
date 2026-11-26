# Clean-room Matrox MSI95 v0.36 build for Windows x86.
# No proprietary Matrox driver source or binaries are used.
$ErrorActionPreference = "Stop"
$sourceRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = (Resolve-Path (Join-Path $sourceRoot "../..")).Path
$build = Join-Path $projectRoot "target/matrox-msi95-x86"
New-Item -ItemType Directory -Force -Path $build | Out-Null

# Use the directly reviewable, hash-locked C source committed to the repo.
$source = (Resolve-Path (Join-Path $sourceRoot "msi95_probe.c")).Path
$expectedSourceHash = "ae50f51304313bddbf718dcd835d2e62defa3bd61a24ceb424bf108188151449"
$sourceHash = (Get-FileHash $source -Algorithm SHA256).Hash.ToLowerInvariant()
if ($sourceHash -cne $expectedSourceHash) { throw "MSI95 source checksum mismatch: $sourceHash" }
$waveHeaderHash = (Get-FileHash (Join-Path $sourceRoot "msi_gpu_batch.h") -Algorithm SHA256).Hash.ToLowerInvariant()
if ($waveHeaderHash -cne "521478141d660739d7f29fba2d8e8c5bb52201d42ef3a2bc56f89d113164f1d9") { throw "GPU wave header checksum mismatch: $waveHeaderHash" }
if ((Get-FileHash (Join-Path $sourceRoot "msi_gpu_draw_batch.h") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "afb6f8a1cadd8e7dee1e73fc93b0f34880e46da1742be9bc2d0c6984aab9513f") { throw "GPU header checksum mismatch: msi_gpu_draw_batch.h" }
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
if ($vkSourceHash -cne "cec20da612f9e8ef08e44c9cdce7f8444ce64f2a0045bb157888470634bfb71b") { throw "Vulkan presenter source checksum mismatch: $vkSourceHash" }
$vkHeaderHash = (Get-FileHash $vkHeader -Algorithm SHA256).Hash.ToLowerInvariant()
if ($vkHeaderHash -cne "238d3ee55866edc0ff105c812454fc453196ee468ae9f68f49eafd07f4c3d9fa") { throw "Vulkan presenter header checksum mismatch: $vkHeaderHash" }
if ((Get-FileHash (Join-Path $sourceRoot "msi_vk_batch_replay.h") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "b886a8f1161feeb45cf8e655ffbb380c7b1b7b74e7ebdad093e8e55a661a51c8") { throw "Shadow source checksum mismatch: msi_vk_batch_replay.h" }
if ((Get-FileHash (Join-Path $sourceRoot "msi_vk_shadow_session.h") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "f4f2e06b85a47f2eaf44f5b53c37e6a73f37464c88b7b1ddb4877f1d706fb611") { throw "Shadow source checksum mismatch: msi_vk_shadow_session.h" }
if ((Get-FileHash (Join-Path $sourceRoot "msi_vk_shadow_bridge.c") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "9427b67637994b75f91695f331bab5145db5e9d4537f829eae16d5a5d531e47d") { throw "Shadow source checksum mismatch: msi_vk_shadow_bridge.c" }
if ((Get-FileHash (Join-Path $sourceRoot "msi_vk_shadow_bridge.h") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "186e0b1970c356cc6464741afc14a4853e46ffb64309527f31ce096434b91b99") { throw "Shadow source checksum mismatch: msi_vk_shadow_bridge.h" }
if ((Get-FileHash (Join-Path $sourceRoot "msi_gpu_compare.h") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "202b14bbe97a6a30154113dbf12f83a738e19c8a43fa3b6b79cb843c18eeaf72") { throw "Shadow source checksum mismatch: msi_gpu_compare.h" }
if ((Get-FileHash (Join-Path $sourceRoot "world_spv_embedded.h") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "cf822014d61d7d25d0d78f13acb0dd7b38ddad9317288f832667a3c2ee46a732") { throw "Shadow source checksum mismatch: world_spv_embedded.h" }
if ((Get-FileHash (Join-Path $sourceRoot "shaders/opaque_world.vert") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "279dd28f45eeab662b6f5ef663b90668b9518085e1dfff711552839a719339c5") { throw "Shadow source checksum mismatch: shaders/opaque_world.vert" }
if ((Get-FileHash (Join-Path $sourceRoot "shaders/opaque_world.frag") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "eca7a42521fcf7ab765a7ecd99a627b18c64a30f6b64412120f695d8a875230e") { throw "Shadow source checksum mismatch: shaders/opaque_world.frag" }
if ((Get-FileHash (Join-Path $sourceRoot "msi_gpu_shadow_sampler.h") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "27b4c493b33f3283466ada32e516f1f5ee5fc808c763fb5666f6cbecc76e4dd9") { throw "Shadow source checksum mismatch: msi_gpu_shadow_sampler.h" }
if ((Get-FileHash (Join-Path $sourceRoot "msi_gpu_publish.h") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "b0046243701d3a62e661bf785ad619efed429f20457fe340de903ee3ff176fd0") { throw "Shadow source checksum mismatch: msi_gpu_publish.h" }
if ((Get-FileHash (Join-Path $sourceRoot "msi_gpu_coverage.h") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "ebb7194bd204cb643229e1fe3006c49374e26329f008f6e80481e5febd30f46f") { throw "Shadow source checksum mismatch: msi_gpu_coverage.h" }
if ((Get-FileHash (Join-Path $sourceRoot "msi_gpu_bypass.h") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "8f4df18326a6aaba36aead2b7121b83f52730c616dd16a03a6a8d08847018725") { throw "Shadow source checksum mismatch: msi_gpu_bypass.h" }
if ((Get-FileHash (Join-Path $sourceRoot "msi_gpu_cpu_replay.h") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "53d9c46a1623731bdadaa3245a1a4cf62668920d5620d069bb4be2a2278e96fe") { throw "CPU replay header checksum mismatch" }
if ((Get-FileHash (Join-Path $sourceRoot "msi_gpu_defer_audit.h") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "840990a8b0cfda4cd1515725aac9f67dcd0b590db607ac1597c35a957acaf43f") { throw "GPU defer audit header checksum mismatch" }
if ((Get-FileHash (Join-Path $sourceRoot "msi_gpu_deferred_finish.h") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "80bf93ee14ac5836d03b523fc3514f37ff3f0dfcb7251087bb1dfcf947e54559") { throw "GPU deferred finish header checksum mismatch" }
if ((Get-FileHash (Join-Path $sourceRoot "msi_gpu_defer_admit.h") -Algorithm SHA256).Hash.ToLowerInvariant() -cne "20efae3b8c88de99e94c7e42d135bc06971a5cad53b0764d1c92ee72d82d5b18") { throw "GPU defer admission header checksum mismatch" }
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
    "CreateFileMappingA@24", "MapViewOfFile@20", "UnmapViewOfFile@4", "GetCurrentThreadId@0",
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
