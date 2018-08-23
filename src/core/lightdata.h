
#ifndef RENDERER_GPU_LIGHTDATA_H
#define RENDERER_GPU_LIGHTDATA_H

#include <optixu/optixu_math_namespace.h>

enum LightType
{
    ENVIRONMENT   = 0,
    POINT = 1,
    DIRECTIONAL = 2
};

struct LightSample
{
    optix::float3 position;
    int           index;
    optix::float3 direction;
    float         distance;
    optix::float3 emission;
    float         pdf;
};


struct LightDefinition
{
    LightType     type;
    // All in world coordinates with no scaling.
    optix::float3 position;
    optix::float3 direction;

    int environmentTextureID;
    float textureScale;
    optix::float3 vecV;
    optix::float3 normal;
    float         area;
    optix::float3 emission;

    // Manual padding to float4 alignment goes here.
    float         unused0;
    float         unused1;

};

#endif //RENDERER_GPU_LIGHTDATA_H
