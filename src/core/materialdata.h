
#ifndef RENDERER_GPU_MATERIALDATA_H
#define RENDERER_GPU_MATERIALDATA_H

#include <optixu/optixu_math_namespace.h>

struct MaterialParameter
{
    unsigned int indexBSDF;  // BSDF index to use in the closest hit program
    optix::float3 albedo;     // Albedo, tint, throughput change for specular surfaces. Pick your meaning.
    int textureID;
    float textureScale;
    optix::float3 absorption; // Absorption coefficient
    float         ior;        // Index of refraction
    unsigned int  flags;      // Thin-walled on/off

    // Manual padding to 16-byte alignment goes here.
    float unused1;
};

#endif //RENDERER_GPU_MATERIALDATA_H
