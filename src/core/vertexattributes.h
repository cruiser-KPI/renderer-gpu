
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
        : vertex(), tangent(optix::make_float3(1.f, 0.f, 0.f)),
        normal(optix::make_float3(0.f, 1.f, 0.f)), texcoord() {}
};

#endif //RENDERER_GPU_VERTEXATTRIBUTES_H
