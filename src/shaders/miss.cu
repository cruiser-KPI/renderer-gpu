
#include <optix.h>
#include <optixu/optixu_math_namespace.h>

#include "../core/perraydata.h"
#include "../core/lightdata.h"
#include "../math/basic.h"

rtBuffer<LightDefinition> sysLightDefinitions;

rtDeclareVariable(optix::Ray, theRay, rtCurrentRay, );
rtDeclareVariable(PerRayData, thePrd, rtPayload, );

RT_PROGRAM void miss_gradient()
{
    LightDefinition light = sysLightDefinitions[0];

    // If the last surface intersection was a diffuse which was directly lit with multiple importance sampling,
    // then calculate light emission with multiple importance sampling as well.

    const float envRotation = light.direction.z * 0.5f;

    const float3 R = theRay.direction;
    // The seam u == 0.0 == 1.0 is in positive z-axis direction.
    // Compensate for the environment rotation done inside the direct lighting.
    const float u     = (atan2f(R.x, -R.z) + M_PIf) * 0.5f * M_1_PIf + envRotation; // DAR FIXME Use a light.matrix to rotate the environment.
    const float theta = acosf(-R.y);     // theta == 0.0f is south pole, theta == M_PIf is north pole.
    const float v     = 1 - theta * M_1_PIf; // Texture is with origin at lower left, v == 0.0f is south pole.

    float3 texColor = make_float3(1.0f);
    if (light.environmentTextureID != RT_TEXTURE_ID_NULL)
        texColor = make_float3(optix::rtTex2D<float4>(light.environmentTextureID, light.textureScale * u, light.textureScale * v));

    const float weightMIS = (thePrd.flags & FLAG_DIFFUSE) ? powerHeuristic(thePrd.pdf, 0.25f * M_1_PIf) : 1.0f;
    thePrd.radiance = make_float3(weightMIS) * light.emission * texColor;

    //TODO proper importance sampling of environment light source

    thePrd.flags |= FLAG_TERMINATE;
}