/* SPDX-License-Identifier: GPL-3.0-or-later
 * Private bounded host storage for the synchronous one-draw qualification
 * pilot. No guest pointers, Vulkan mappings, allocation on ordinary frames,
 * or persistent texture identities. The borrower resets metadata and fully
 * overwrites its baseline/decoded/packed ranges before reading them.
 */
#ifndef MSI_GPU_PILOT_STORAGE_H
#define MSI_GPU_PILOT_STORAGE_H
#include <stdint.h>
#include <stddef.h>
#define MSI_GPU_PILOT_NATIVE_BYTES 512000u
#define MSI_GPU_PILOT_RAW_BYTES (1024u*1024u+512u)
#define MSI_GPU_PILOT_DECODED_COUNT (1024u*1024u)
#define MSI_GPU_PILOT_PACKED_COUNT (640u*400u)
typedef struct {
    uint8_t *before,*raw;
    uint32_t *decoded,*packed;
} MsiGpuPilotStorage;
typedef void *(*MsiGpuPilotAlloc)(unsigned);
typedef void (*MsiGpuPilotFree)(void *);
static inline void msi_gpu_pilot_storage_destroy(MsiGpuPilotStorage *s,MsiGpuPilotFree release){
    if(s->before)release(s->before);
    if(s->raw)release(s->raw);
    if(s->decoded)release(s->decoded);
    if(s->packed)release(s->packed);
    s->before=s->raw=0;s->decoded=s->packed=0;
}
/* Return 1 for a new allocation, 2 for reuse, 0 for failure. All four
 * allocations must succeed before publishing ownership; partial failures
 * leave the old empty storage untouched. */
static inline unsigned msi_gpu_pilot_storage_acquire(MsiGpuPilotStorage *s,MsiGpuPilotAlloc alloc,MsiGpuPilotFree release){
    if(s->before||s->raw||s->decoded||s->packed)
        return s->before&&s->raw&&s->decoded&&s->packed?2u:0u;
    MsiGpuPilotStorage n={0};
    n.before=(uint8_t*)alloc(MSI_GPU_PILOT_NATIVE_BYTES);
    if(!n.before)goto fail;
    n.raw=(uint8_t*)alloc(MSI_GPU_PILOT_RAW_BYTES);
    if(!n.raw)goto fail;
    n.decoded=(uint32_t*)alloc(MSI_GPU_PILOT_DECODED_COUNT*4u);
    if(!n.decoded)goto fail;
    n.packed=(uint32_t*)alloc(MSI_GPU_PILOT_PACKED_COUNT*4u);
    if(!n.packed)goto fail;
    *s=n;return 1u;
fail:
    msi_gpu_pilot_storage_destroy(&n,release);
    return 0u;
}
/* Exactly 512000 bytes; nonoverlapping caller-owned destination. x86
 * string move avoids an external CRT memcpy and handles unaligned bytes.
 * No SIMD feature requirement, no changes to guest protection or identity. */
static inline void msi_gpu_native_copy(uint8_t *dst,const uint8_t *src){
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__i386__) || defined(__x86_64__))
    size_t words=MSI_GPU_PILOT_NATIVE_BYTES/4u;
    __asm__ volatile("cld; rep movsl" : "+D"(dst), "+S"(src), "+c"(words) : : "memory","cc");
#else
    for(size_t i=0;i<MSI_GPU_PILOT_NATIVE_BYTES;i++)dst[i]=src[i];
#endif
}
#endif
