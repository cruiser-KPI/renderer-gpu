

#include <optix.h>
#include <optixu/optixu_math_namespace.h>

// Note, the nomenclature used in the device code of all optixIntroduction samples
// follows some simple rules using prefixes to help indicating the scope and meaning:
//
// "sys" = renderer "system"-wide variables, defined at global context scope.
// "the" = variables with OptiX built-in semantic, like rtLaunchIndex, etc.
// "var" = "varyings" with developer defined attribute semantic, calculated by the intersection program.
// "par" = "parameter" variable held at some object scope, not at the global context scope.
//         (Exception to the last rule are the vertex "attributes" and "indices" held at Geometry nodes.)

#include "../utils/config.h"
#include "../math/basic.h"

#include "../core/perraydata.h"

rtBuffer<float4,  2> sysOutputBuffer; // RGBA32F

rtDeclareVariable(rtObject, sysTopObject, , );
rtDeclareVariable(float,    sysSceneEpsilon, , );
rtDeclareVariable(int2,     sysPathLengths, , );
rtDeclareVariable(int,      sysIterationIndex, , );

rtDeclareVariable(float3, sysCameraPosition, , );
rtDeclareVariable(float3, sysCameraU, , );
rtDeclareVariable(float3, sysCameraV, , );
rtDeclareVariable(float3, sysCameraW, , );

rtDeclareVariable(uint2, theLaunchDim,   rtLaunchDim, );
rtDeclareVariable(uint2, theLaunchIndex, rtLaunchIndex, );

rtDeclareVariable(uint2, tileOffset, , );
rtDeclareVariable(uint2, resolution, , );

RT_FUNCTION void integrator(PerRayData& prd, float3& radiance)
{
    radiance = make_float3(0.0f); // Start with black.

    float3 throughput = make_float3(1.0f); // The throughput for the next radiance, starts with 1.0f.

    int depth = 0; // Path segment index. Primary ray is 0.

    while (depth < sysPathLengths.y)
    {
        prd.wo    = -prd.wi; // wi is the next path segment ray.direction. wo is the direction to the observer.
        prd.flags = 0;       // Clear all non-persistent flags. None in this version.

        // Note that the primary rays wouldn't need to offset the ray t_min by sysSceneEpsilon.
        optix::Ray ray = optix::make_Ray(prd.pos, prd.wi, 0, sysSceneEpsilon, RT_DEFAULT_MAX);
        rtTrace(sysTopObject, ray, prd);

        radiance += throughput * prd.radiance;

        // Path termination by miss shader or sample() routines.
        // If terminate is true, f_over_pdf and pdf might be undefined.
        if ((prd.flags & FLAG_TERMINATE) || prd.pdf <= 0.0f || isNull(prd.f_over_pdf))
        {
            break;
        }

        // PERF f_over_pdf already contains the proper throughput adjustment for diffuse materials: f * (fabsf(optix::dot(prd.wi, state.normal)) / prd.pdf);
        throughput *= prd.f_over_pdf;

        // Unbiased Russian Roulette path termination.
        if (sysPathLengths.x <= depth) // Start termination after a minimum number of bounces.
        {
            const float probability = fmaxf(throughput); // DAR Other options: // intensity(throughput); // fminf(0.5f, intensity(throughput));
            if (probability < rng(prd.seed)) // Paths with lower probability to continue are terminated earlier.
            {
                break;
            }
            throughput /= probability; // Path isn't terminated. Adjust the throughput so that the average is right again.
        }

        ++depth; // Next path segment.
    }
}

// Entry point for pinhole camera with manual accumulation, non-VCA.
RT_PROGRAM void raygeneration()
{
    PerRayData prd;

    const uint2 tileLaunchIndex = tileOffset + theLaunchIndex;
    prd.seed = tea<8>(tileLaunchIndex.y * resolution.x + tileLaunchIndex.x, sysIterationIndex);

    const float2 pixel = make_float2(tileLaunchIndex);
    // Sample the ray in the center of the pixel.
    const float2 fragment = pixel + rng2(prd.seed); // Random jitter of the fragment location in this pixel.
    // The launch dimension (set with rtContextLaunch) is the full client window in this demo's setup.
    const float2 screen = make_float2(resolution);
    // Normalized device coordinates in range [-1, 1].
    const float2 ndc = (fragment / screen) * 2.0f - 1.0f;

    // The integrator expects the next path segments ray.origin in prd.pos and the next ray.direction in prd.wi.
    prd.pos = sysCameraPosition;
    prd.wi  = optix::normalize(ndc.x * sysCameraU + ndc.y * sysCameraV + sysCameraW);

    float3 radiance;

    integrator(prd, radiance); // In this case a unidirectional path tracer.

#ifdef USE_DEBUG_EXCEPTIONS
    // DAR DEBUG Highlight numerical errors.
  if (isnan(radiance.x) || isnan(radiance.y) || isnan(radiance.z))
  {
    radiance = make_float3(1000000.0f, 0.0f, 0.0f); // super red
  }
  else if (isinf(radiance.x) || isinf(radiance.y) || isinf(radiance.z))
  {
    radiance = make_float3(0.0f, 1000000.0f, 0.0f); // super green
  }
  else if (radiance.x < 0.0f || radiance.y < 0.0f || radiance.z < 0.0f)
  {
    radiance = make_float3(0.0f, 0.0f, 1000000.0f); // super blue
  }
#else
    // NaN values will never go away. Filter them out before they can arrive in the output buffer.
    // This only has an effect if the debug coloring above is off!
    if (!(isnan(radiance.x) || isnan(radiance.y) || isnan(radiance.z)))
#endif
    {
        if (0 < sysIterationIndex)
        {
            float4 dst = sysOutputBuffer[tileLaunchIndex];  // RGBA32F
            sysOutputBuffer[tileLaunchIndex] = optix::lerp(dst, make_float4(radiance, 1.0f), 1.0f / (float) (sysIterationIndex + 1));
        }
        else
        {
            // sysIterationIndex 0 will fill the buffer.
            // If this isn't done separately, the result of the lerp() above is undefined, e.g. dst could be NaN.
            sysOutputBuffer[tileLaunchIndex] = make_float4(radiance, 1.0f);
        }
    }
}
