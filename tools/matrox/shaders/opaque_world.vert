#version 450
// SPDX-License-Identifier: GPL-3.0-or-later
// First offscreen proof: pre-transformed MSI vertices, opaque world only.
// Deliberately keep w=1 and interpolate UV*invW and invW separately.
layout(location=0) in vec2 inXY;
layout(location=1) in vec2 inUV;
layout(location=2) in float inInvW;
layout(location=3) in vec3 inModifier;
layout(location=0) noperspective out vec2 uvWeighted;
layout(location=1) noperspective out float invW;
layout(location=2) noperspective out vec3 modifier;
layout(push_constant) uniform Draw {
    vec2 logicalExtent; // 640,400
    vec2 targetExtent;  // independently sized offscreen render target
    uint flags;
    uint reserved;
} draw;
void main() {
    gl_Position=vec4(inXY/draw.logicalExtent*2.0-1.0,0.0,1.0);
    invW=(draw.flags&1u)!=0u?inInvW:1.0;
    uvWeighted=inUV*invW;
    modifier=inModifier;
}
