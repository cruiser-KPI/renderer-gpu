
#ifndef RENDERER_GPU_BASIC_H
#define RENDERER_GPU_BASIC_H

#include "../utils/config.h"

#include <optix.h>
#include <optixu/optixu_math_namespace.h>
using namespace optix;

// Explicitly not named Onb to not conflict with the optix::Onb
// Tangent-Bitangent-Normal orthonormal space.
struct TBN
{
    // Default constructor to be able to include it into other structures when needed.
    RT_FUNCTION TBN()
    {
    }

    RT_FUNCTION TBN(const optix::float3& n)
        : normal(n)
    {
        if (fabsf(normal.z) < fabsf(normal.x))
        {
            tangent.x =  normal.z;
            tangent.y =  0.0f;
            tangent.z = -normal.x;
        }
        else
        {
            tangent.x =  0.0f;
            tangent.y =  normal.z;
            tangent.z = -normal.y;
        }
        tangent   = optix::normalize(tangent);
        bitangent = optix::cross(normal, tangent);
    }

    // Constructor for cases where tangent, bitangent, and normal are given as ortho-normal basis.
    RT_FUNCTION TBN(const optix::float3& t, const optix::float3& b, const optix::float3& n)
        : tangent(t)
        , bitangent(b)
        , normal(n)
    {
    }

    // Normal is kept, tangent and bitangent are calculated.
    // Normal must be normalized.
    // Must not be used with degenerated vectors!
    RT_FUNCTION TBN(const optix::float3& tangent_reference, const optix::float3& n)
        : normal(n)
    {
        bitangent = optix::normalize(optix::cross(normal, tangent_reference));
        tangent   = optix::cross(bitangent, normal);
    }

    RT_FUNCTION void negate()
    {
        tangent   = -tangent;
        bitangent = -bitangent;
        normal    = -normal;
    }

    RT_FUNCTION optix::float3 transform(const optix::float3& p) const
    {
        return optix::make_float3(optix::dot(p, tangent),
                                  optix::dot(p, bitangent),
                                  optix::dot(p, normal));
    }

    RT_FUNCTION optix::float3 inverse_transform(const optix::float3& p) const
    {
        return p.x * tangent + p.y * bitangent + p.z * normal;
    }

    optix::float3 tangent;
    optix::float3 bitangent;
    optix::float3 normal;
};


//RT_FUNCTION float luminance(const optix::float3& rgb)
//{
//  const optix::float3 ntsc_luminance = { 0.30f, 0.59f, 0.11f };
//  return optix::dot(rgb, ntsc_luminance);
//}

RT_FUNCTION float intensity(const optix::float3& rgb)
{
    return (rgb.x + rgb.y + rgb.z) * 0.3333333333f;
}

RT_FUNCTION float intensity3(const optix::float4& rgb)
{
    return (rgb.x + rgb.y + rgb.z) * 0.3333333333f;
}

RT_FUNCTION float cube(const float x)
{
    return x * x * x;
}

// Helper functions.
RT_FUNCTION optix::float3 logf(const optix::float3& v)
{
    return optix::make_float3(::logf(v.x), ::logf(v.y), ::logf(v.z));
}

RT_FUNCTION optix::float2 floorf(const optix::float2& v)
{
    return optix::make_float2(::floorf(v.x), ::floorf(v.y));
}

RT_FUNCTION optix::float3 floorf(const optix::float3& v)
{
    return optix::make_float3(::floorf(v.x), ::floorf(v.y), ::floorf(v.z));
}

RT_FUNCTION optix::float3 ceilf(const optix::float3& v)
{
    return optix::make_float3(::ceilf(v.x), ::ceilf(v.y), ::ceilf(v.z));
}

RT_FUNCTION optix::float3 powf(const optix::float3& v, const float e)
{
    return optix::make_float3(::powf(v.x, e), ::powf(v.y, e), ::powf(v.z, e));
}

RT_FUNCTION optix::float4 powf(const optix::float4& v, const float e)
{
    return optix::make_float4(::powf(v.x, e), ::powf(v.y, e), ::powf(v.z, e), ::powf(v.w, e));
}


RT_FUNCTION optix::float2 fminf(const optix::float2& v, const float m)
{
    return optix::make_float2(::fminf(v.x, m), ::fminf(v.y, m));
}
RT_FUNCTION optix::float3 fminf(const optix::float3& v, const float m)
{
    return optix::make_float3(::fminf(v.x, m), ::fminf(v.y, m), ::fminf(v.z, m));
}
RT_FUNCTION optix::float4 fminf(const optix::float4& v, const float m)
{
    return optix::make_float4(::fminf(v.x, m), ::fminf(v.y, m), ::fminf(v.z, m), ::fminf(v.w, m));
}

RT_FUNCTION optix::float2 fminf(const float m, const optix::float2& v)
{
    return optix::make_float2(::fminf(m, v.x), ::fminf(m, v.y));
}
RT_FUNCTION optix::float3 fminf(const float m, const optix::float3& v)
{
    return optix::make_float3(::fminf(m, v.x), ::fminf(m, v.y), ::fminf(m, v.z));
}
RT_FUNCTION optix::float4 fminf(const float m, const optix::float4& v)
{
    return optix::make_float4(::fminf(m, v.x), ::fminf(m, v.y), ::fminf(m, v.z), ::fminf(m, v.w));
}


RT_FUNCTION optix::float2 fmaxf(const optix::float2& v, const float m)
{
    return optix::make_float2(::fmaxf(v.x, m), ::fmaxf(v.y, m));
}
RT_FUNCTION optix::float3 fmaxf(const optix::float3& v, const float m)
{
    return optix::make_float3(::fmaxf(v.x, m), ::fmaxf(v.y, m), ::fmaxf(v.z, m));
}
RT_FUNCTION optix::float4 fmaxf(const optix::float4& v, const float m)
{
    return optix::make_float4(::fmaxf(v.x, m), ::fmaxf(v.y, m), ::fmaxf(v.z, m), ::fmaxf(v.w, m));
}

RT_FUNCTION optix::float2 fmaxf(const float m, const optix::float2& v)
{
    return optix::make_float2(::fmaxf(m, v.x), ::fmaxf(m, v.y));
}
RT_FUNCTION optix::float3 fmaxf(const float m, const optix::float3& v)
{
    return optix::make_float3(::fmaxf(m, v.x), ::fmaxf(m, v.y), ::fmaxf(m, v.z));
}
RT_FUNCTION optix::float4 fmaxf(const float m, const optix::float4& v)
{
    return optix::make_float4(::fmaxf(m, v.x), ::fmaxf(m, v.y), ::fmaxf(m, v.z), ::fmaxf(m, v.w));
}


RT_FUNCTION bool isNull(const optix::float3& v)
{
    return (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f);
}

RT_FUNCTION bool isNotNull(const optix::float3& v)
{
    return (v.x != 0.0f || v.y != 0.0f || v.z != 0.0f);
}

// Used for Multiple Importance Sampling.
RT_FUNCTION float powerHeuristic(const float a, const float b)
{
    const float t = a * a;
    return t / (t + b * b);
}

RT_FUNCTION float balanceHeuristic(const float a, const float b)
{
    return a / (a + b);
}

RT_FUNCTION float evaluateFresnelDielectric(const float et, const float cosIn)
{
    const float cosi = fabsf(cosIn);

    float sint = 1.0f - cosi * cosi;
    sint = (0.0f < sint) ? sqrtf(sint) / et : 0.0f;

    // Handle total internal reflection.
    if (1.0f < sint)
    {
        return 1.0f;
    }

    float cost = 1.0f - sint * sint;
    cost = (0.0f < cost) ? sqrtf(cost) : 0.0f;

    const float et_cosi = et * cosi;
    const float et_cost = et * cost;

    const float rPerpendicular = (cosi - et_cost) / (cosi + et_cost);
    const float rParallel      = (et_cosi - cost) / (et_cosi + cost);

    const float result = (rParallel * rParallel + rPerpendicular * rPerpendicular) * 0.5f;

    return (result <= 1.0f) ? result : 1.0f;
}


RT_FUNCTION void alignVector(float3 const& axis, float3& w)
{
    // Align w with axis.
    const float s = copysign(1.0f, axis.z);
    w.z *= s;
    const float3 h = make_float3(axis.x, axis.y, axis.z + s);
    const float  k = optix::dot(w, h) / (1.0f + fabsf(axis.z));
    w = k * h - w;
}

RT_FUNCTION void unitSquareToCosineHemisphere(const float2 sample, float3 const& axis, float3& w, float& pdf)
{
    // Choose a point on the hemisphere about +z
    const float theta = 2.0f * M_PIf * sample.x;
    const float r = sqrtf(sample.y);
    w.x = r * cosf(theta);
    w.y = r * sinf(theta);
    w.z = 1.0f - w.x * w.x - w.y * w.y;
    w.z = (0.0f < w.z) ? sqrtf(w.z) : 0.0f;

    pdf = w.z * M_1_PIf;

    // Align with axis.
    alignVector(axis, w);
}

RT_FUNCTION void unitSquareToSphere(const float u, const float v, float3& p, float& pdf)
{
    p.z = 1.0f - 2.0f * u;
    float r = 1.0f - p.z * p.z;
    r = (0.0f < r) ? sqrtf(r) : 0.0f;

    const float phi = v * 2.0f * M_PIf;
    p.x = r * cosf(phi);
    p.y = r * sinf(phi);

    pdf = 0.25f * M_1_PIf;  // == 1.0f / (4.0f * M_PIf)
}

#endif //RENDERER_GPU_BASIC_H
