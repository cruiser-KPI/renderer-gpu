

#include <optix.h>
#include <optixu/optixu_aabb_namespace.h>
#include <optixu/optixu_math_namespace.h>

#include "../core/vertexattributes.h"

rtBuffer<VertexAttributes> attributesBuffer;

RT_PROGRAM void triangle_bbox(int primitiveIndex, float result[6])
{
    const float3 v0 = attributesBuffer[3*primitiveIndex  ].vertex;
    const float3 v1 = attributesBuffer[3*primitiveIndex+1].vertex;
    const float3 v2 = attributesBuffer[3*primitiveIndex+2].vertex;

    const float area = optix::length(optix::cross(v1 - v0, v2 - v0));

    optix::Aabb *aabb = (optix::Aabb *) result;

    if (0.0f < area && !isinf(area))
    {
        aabb->m_min = fminf(fminf(v0, v1), v2);
        aabb->m_max = fmaxf(fmaxf(v0, v1), v2);
    }
    else
    {
        aabb->invalidate();
    }
}