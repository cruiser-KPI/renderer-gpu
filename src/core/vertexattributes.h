
#ifndef RENDERER_GPU_VERTEXATTRIBUTES_H
#define RENDERER_GPU_VERTEXATTRIBUTES_H

#include <optixu/optixu_math_namespace.h>

struct VertexAttributes
{
    optix::float3 vertex;
    optix::float3 tangent;
    optix::float3 normal;
    optix::float3 texcoord;
};

#endif //RENDERER_GPU_VERTEXATTRIBUTES_H
