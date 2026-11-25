/* SPDX-License-Identifier: GPL-3.0-or-later
 * Recorded opaque indexed8 CPU fallback proof. All input comes from immutable
 * snapshots; no live parameters/cache or guest pointers. Output is private
 * scratch, which must be discarded on error. MSI95 uses it only as a diagnostic.
 * Explicit float32 rounding preserves legacy incremental scanline arithmetic
 * even with x87 excess precision. Never substitute direct barycentrics.
 */
#ifndef MSI_GPU_CPU_REPLAY_H
#define MSI_GPU_CPU_REPLAY_H
#include "msi_gpu_draw_batch.h"
#include "msi_world_bilinear.h"
static inline float mcr_f(float x){volatile float r=x;return r;}
static inline float mcr_cross(float ax,float ay,float bx,float by){
    return mcr_f(mcr_f(ax*by)-mcr_f(ay*bx));
}
static inline float mcr_interp(float q0,float q1,float q2,float a,float b,float c){
    return mcr_f(mcr_f(mcr_f(q0*a)+mcr_f(q1*b))+mcr_f(q2*c));
}
static inline unsigned mcr_mod(float x){
    if(x<0.f)x=0.f;if(x>255.f)x=255.f;
    return (unsigned)mcr_f(x+.5f);
}
static inline int mcr_floor(float x){int i=(int)x;return i-(x<(float)i);}
static inline unsigned mcr_address(int i,unsigned n,unsigned clamp){
    if(clamp)return i<0?0u:(unsigned)i>=n?n-1u:(unsigned)i;
    int r=i%(int)n;return (unsigned)(r<0?r+(int)n:r);
}
static inline unsigned mcr_light(unsigned v,unsigned m,unsigned cap,unsigned dither,unsigned threshold){
    unsigned n=v*m;
    if(!dither)return (n+127u)/255u;
    unsigned q=n/255u+(n%255u>threshold);
    return q>cap?cap:q;
}
static inline int msi_gpu_cpu_replay(const MsiGpuDrawBatch *b,unsigned w,unsigned h,
    const uint16_t *before,uint16_t *out,uint8_t *written){
    if(!b||!before||!out||before==out||!written||!w||!h||w>4096u||h>4096u||
       !b->count||b->count>32u||!b->snapshots.decoded||!b->snapshots.count||
       b->snapshots.count>MSI_GPU_SNAPSHOT_MAX||b->snapshots.decoded_used>2097152u)return 0;
    for(unsigned k=0;k<b->count;k++){
        const MsiGpuDraw *d=&b->draws[k];MsiGpuRect r=d->bounds;
        if(d->texture_id>=b->snapshots.count||(d->flags&~127u)||
           r.x0>=r.x1||r.y0>=r.y1||r.x1>w||r.y1>h)return 0;
        const MsiGpuSnapshot *s=&b->snapshots.entries[d->texture_id];
        unsigned tw=s->state.width,th=s->state.height;
        if(!tw||!th||tw>2048u||th>2048u||tw*th>1048576u||
           s->decoded_count!=(size_t)tw*th||s->decoded_offset>b->snapshots.decoded_used||
           s->decoded_count>b->snapshots.decoded_used-s->decoded_offset)return 0;
        for(unsigned i=0;i<3u;i++)if(!msi_gpu_vertex_valid(&b->vertices[3u*k+i],d->flags))return 0;
    }
    for(size_t i=0;i<b->snapshots.decoded_used;i++)if(b->snapshots.decoded[i]&~0x3ffffu)return 0;
    for(size_t j=0;j<(size_t)w*h;j++){out[j]=before[j];written[j]=0;}
    const unsigned bayer[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
    for(unsigned k=0;k<b->count;k++){
        const MsiGpuDraw *d=&b->draws[k];MsiGpuRect r=d->bounds;
        unsigned f=d->flags,cu=f&2u,cv=f&4u,key=f&64u;
        const MsiGpuSnapshot *s=&b->snapshots.entries[d->texture_id];
        unsigned tw=s->state.width,th=s->state.height;
        const uint32_t *tex=b->snapshots.decoded+s->decoded_offset;
        const MsiGpuVertex *a=&b->vertices[3u*k],*v=a+1,*c=a+2;
        float bx=mcr_f(v->xy[0]-a->xy[0]),by=mcr_f(v->xy[1]-a->xy[1]);
        float cx=mcr_f(c->xy[0]-a->xy[0]),cy=mcr_f(c->xy[1]-a->xy[1]);
        float den=mcr_cross(bx,by,cx,cy);
        if(den>-.0001f&&den<.0001f)continue;
        float inv=mcr_f(1.f/den),step1=mcr_f(cy*inv),step2=mcr_f(-by*inv);
        float au=mcr_f(a->uv[0]*a->iw),bu=mcr_f(v->uv[0]*v->iw),cu_w=mcr_f(c->uv[0]*c->iw);
        float av=mcr_f(a->uv[1]*a->iw),bv=mcr_f(v->uv[1]*v->iw),cv_w=mcr_f(c->uv[1]*c->iw);
        unsigned constant=1;for(unsigned i=0;i<3u;i++)constant&=a->m[i]==v->m[i]&&a->m[i]==c->m[i];
        for(unsigned y=r.y0;y<r.y1;y++){
            float px=mcr_f((float)r.x0+.5f-a->xy[0]),py=mcr_f((float)y+.5f-a->xy[1]);
            float w1=mcr_f(mcr_cross(px,py,cx,cy)*inv),w2=mcr_f(mcr_cross(bx,by,px,py)*inv);
            for(unsigned x=r.x0;x<r.x1;x++){
                float q1=w1,q2=w2,q0=mcr_f(mcr_f(1.f-q1)-q2);
                w1=mcr_f(w1+step1);w2=mcr_f(w2+step2);
                if(q0<-.00001f||q1<-.00001f||q2<-.00001f)continue;
                float u=mcr_interp(q0,q1,q2,a->uv[0],v->uv[0],c->uv[0]);
                float tv=mcr_interp(q0,q1,q2,a->uv[1],v->uv[1],c->uv[1]);
                if(f&1u){
                    float iw=mcr_interp(q0,q1,q2,a->iw,v->iw,c->iw);
                    if(iw>1e-8f&&iw<1e8f){
                        u=mcr_f(mcr_interp(q0,q1,q2,au,bu,cu_w)/iw);
                        tv=mcr_f(mcr_interp(q0,q1,q2,av,bv,cv_w)/iw);
                    }
                }
                /* Reject unbounded/NaN coordinates before any integer cast. */
                if(!(u>=-1000000.f&&u<=1000000.f&&tv>=-1000000.f&&tv<=1000000.f))return 0;
                if(cu)u=u<0.f?0.f:u>1.f?1.f:u;else u=mcr_f(u-(float)mcr_floor(u));
                if(cv)tv=tv<0.f?0.f:tv>1.f?1.f:tv;else tv=mcr_f(tv-(float)mcr_floor(tv));
                unsigned tx=(unsigned)mcr_f(u*tw),ty=(unsigned)mcr_f(tv*th);
                if(tx>=tw)tx=tw-1u;if(ty>=th)ty=th-1u;
                unsigned center=tex[ty*tw+tx],color=center&65535u;
                if(key&&(center&65536u))continue;
                if(f&8u){
                    float sx=mcr_f(mcr_f(u*tw)-.5f),sy=mcr_f(mcr_f(tv*th)-.5f);
                    int ix=mcr_floor(sx),iy=mcr_floor(sy);
                    unsigned wx=(unsigned)mcr_f(mcr_f(mcr_f(sx-ix)*256.f)+.5f);
                    unsigned wy=(unsigned)mcr_f(mcr_f(mcr_f(sy-iy)*256.f)+.5f);
                    unsigned x0=mcr_address(ix,tw,cu),x1=mcr_address(ix+1,tw,cu);
                    unsigned y0=mcr_address(iy,th,cv),y1=mcr_address(iy+1,th,cv);
                    unsigned t0=tex[y0*tw+x0],t1=tex[y0*tw+x1],t2=tex[y1*tw+x0],t3=tex[y1*tw+x1];
                    if(!key||!((t0|t1|t2|t3)&0x30000u))
                        color=msi_bilinear_mix_rgb565(t0&65535u,t1&65535u,t2&65535u,t3&65535u,wx,wy);
                }
                if(f&16u){
                    unsigned m[3];for(unsigned i=0;i<3u;i++)m[i]=mcr_mod(constant?a->m[i]:mcr_interp(q0,q1,q2,a->m[i],v->m[i],c->m[i]));
                    unsigned threshold=(2u*bayer[(y&3u)*4u+(x&3u)]+1u)*255u/32u;
                    color=(mcr_light(color>>11,m[0],31u,f&32u,threshold)<<11)|
                        (mcr_light((color>>5)&63u,m[1],63u,f&32u,threshold)<<5)|
                        mcr_light(color&31u,m[2],31u,f&32u,threshold);
                }
                size_t j=(size_t)y*w+x;out[j]=(uint16_t)color;written[j]=key?2u:1u;
            }
        }
    }
    return 1;
}
#endif
