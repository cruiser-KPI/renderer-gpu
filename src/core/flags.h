
#ifndef RENDERER_GPU_FLAGS_H
#define RENDERER_GPU_FLAGS_H

// Set when reaching a closesthit program. Unused in this demo
#define FLAG_HIT            0x00000001
// Set by BSDFs which support direct lighting. Not set means specular interaction. Cleared in the closesthit program.
// Used to decide when to do direct lighting and multuiple importance sampling on implicit light hits.
#define FLAG_DIFFUSE        0x00000002

// Set if (0.0f <= wo_dot_ng), means looking onto the front face. (Edge-on is explicitly handled as frontface for the material stack.)
#define FLAG_FRONTFACE      0x00000010
// Pass down material.flags through to the BSDFs.
#define FLAG_THINWALLED     0x00000020

// FLAG_TRANSMISSION is set if there is a transmission. (Can't happen when FLAG_THINWALLED is set.)
#define FLAG_TRANSMISSION   0x00000100
// Set if the material stack is not empty.
#define FLAG_VOLUME         0x00001000

// Highest bit set means terminate path.
#define FLAG_TERMINATE      0x80000000

#endif //RENDERER_GPU_FLAGS_H
