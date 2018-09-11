
#ifndef RENDERER_GPU_MATERIALDATA_H
#define RENDERER_GPU_MATERIALDATA_H

#include <optixu/optixu_math_namespace.h>

enum MaterialType
{
    DIFFUSE = 0,
    GLOSSY = 1,
    REFRACTION = 2,
    GLASS = 3,
    MIX = 4
};


struct MaterialParameter
{
    unsigned int indexBSDF = 0;  // BSDF index to use in the closest hit program
    optix::float3 albedo;
    float roughness = 0.0f;
    float anisotropy = 0.0f;
    float rotation = 0.0f;
    int textureID = RT_TEXTURE_ID_NULL;
    float textureScale = 1.0f;
    float ior = 1.5f;
    unsigned int flags = 0;

    float unused0;
};

#endif //RENDERER_GPU_MATERIALDATA_H
