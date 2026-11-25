/* SPDX-License-Identifier: GPL-3.0-or-later
 * Passive audit of public MSI framebuffer pointer state and conservative
 * homogeneous batch opportunities. No deferral, page protection, or guest
 * memory writes. An invalidated public pointer does NOT prove absence of
 * aliases, threads, or direct reads. All numbers are sampled opportunities.
 */
#ifndef MSI_GPU_DEFER_AUDIT_H
#define MSI_GPU_DEFER_AUDIT_H
enum {MSI_DEFER_MAPPED=0,MSI_DEFER_INVALIDATED=1,MSI_DEFER_UNKNOWN=2};
enum {MSI_DEFER_LIMIT=32};
typedef struct {
    unsigned eligible,mapped,invalidated,unknown,excluded;
    unsigned pending,key,groups,group_draws,multi_groups,multi_draws,max_group;
    unsigned split_key,split_limit,barriers,remaps,remaps_invalidated;
} MsiGpuDeferAudit;
static inline unsigned msi_gpu_defer_pointer(unsigned color,unsigned depth,
    unsigned backing_color,unsigned backing_depth){
    if(color==0xdeadbeefu&&depth==0xdeadbeefu)return MSI_DEFER_INVALIDATED;
    if(backing_color&&backing_depth&&color==backing_color&&depth==backing_depth)return MSI_DEFER_MAPPED;
    return MSI_DEFER_UNKNOWN;
}
static inline void msi_gpu_defer_flush(MsiGpuDeferAudit *a){
    if(!a->pending)return;
    a->groups++;a->group_draws+=a->pending;
    if(a->pending>1u){a->multi_groups++;a->multi_draws+=a->pending;}
    if(a->pending>a->max_group)a->max_group=a->pending;
    a->pending=0;
}
static inline void msi_gpu_defer_barrier(MsiGpuDeferAudit *a){
    if(a->pending)a->barriers++;
    msi_gpu_defer_flush(a);
}
static inline void msi_gpu_defer_candidate(MsiGpuDeferAudit *a,unsigned status,
    unsigned key,int eligible){
    if(!eligible){a->excluded++;msi_gpu_defer_barrier(a);return;}
    a->eligible++;
    if(status==MSI_DEFER_MAPPED)a->mapped++;
    else if(status==MSI_DEFER_INVALIDATED)a->invalidated++;
    else a->unknown++;
    if(status!=MSI_DEFER_INVALIDATED){msi_gpu_defer_barrier(a);return;}
    key=!!key;
    if(a->pending&&key!=a->key){a->split_key++;msi_gpu_defer_flush(a);}
    if(a->pending==MSI_DEFER_LIMIT){a->split_limit++;msi_gpu_defer_flush(a);}
    a->key=key;a->pending++;
}
static inline int msi_gpu_defer_counts_ok(const MsiGpuDeferAudit *a){
    return a->eligible==a->mapped+a->invalidated+a->unknown&&
        a->invalidated==a->group_draws+a->pending&&a->multi_draws<=a->group_draws&&
        a->multi_groups<=a->groups&&a->max_group<=MSI_DEFER_LIMIT&&
        a->pending<=MSI_DEFER_LIMIT&&a->remaps_invalidated<=a->remaps;
}
#endif
