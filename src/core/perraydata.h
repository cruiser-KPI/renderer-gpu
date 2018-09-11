
#ifndef RENDERER_GPU_PERRAYDATA_H
#define RENDERER_GPU_PERRAYDATA_H

#include <optixu/optixu_math_namespace.h>

#include "../math/rng.h"
#include "flags.h"


struct State
{
    optix::float3 geoNormal;
    optix::float3 normal;
    optix::float3 texcoord;
    optix::float3 tangent;
    optix::float3 bitangent;
};


// Note that the fields are ordered by CUDA alignment.
struct PerRayData
{
    optix::float4 absorption_ior; // The absorption coefficient and IOR of the currently hit material.
    optix::float2 ior;            // .x = IOR the ray currently is inside, .y = the IOR of the surrounding volume. The IOR of the current material is in absorption_ior.w!

    optix::float3 pos;            // Current surface hit point or volume sample point, in world space
    float         distance;       // Distance from the ray origin to the current position, in world space. Needed for absorption of nested materials.

    optix::float3 wo;             // Outgoing direction, to observer, in world space.
    optix::float3 wi;             // Incoming direction, to light, in world space.

    optix::float3 radiance;       // Radiance along the current path segment.
    int           flags;          // Bitfield with flags. See FLAG_* defines for its contents.

    optix::float3 f_over_pdf;     // BSDF sample throughput, pre-multiplied f_over_pdf = bsdf.f * fabsf(dot(wi, ns) / bsdf.pdf;
    float         pdf;            // The last BSDF sample's pdf, tracked for multiple importance sampling.

    optix::float3 extinction;     // The current volume's extinction coefficient. (Only absorption in this implementation.)

    unsigned int  seed;           // Random number generator input.
};

struct PerRayData_shadow
{
    bool visible;
};

#endif //RENDERER_GPU_PERRAYDATA_H
