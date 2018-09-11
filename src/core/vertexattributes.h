
#ifndef RENDERER_GPU_VERTEXATTRIBUTES_H
#define RENDERER_GPU_VERTEXATTRIBUTES_H

#include <optixu/optixu_math_namespace.h>

struct VertexAttributes
{
    optix::float3 vertex;
    optix::float3 tangent;
    optix::float3 normal;
    optix::float3 texcoord;

    VertexAttributes()
        : vertex(), tangent(optix::make_float3(.0f)),
        normal(optix::make_float3(.0f)), texcoord(optix::make_float3(.0f)) {}
};

#endif //RENDERER_GPU_VERTEXATTRIBUTES_H
