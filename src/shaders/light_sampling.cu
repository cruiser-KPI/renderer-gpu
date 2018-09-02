

#include <optix.h>
#include <optixu/optixu_math_namespace.h>

#include "../utils/config.h"
#include "../core/perraydata.h"
#include "../core/lightdata.h"
#include "../math/basic.h"

rtBuffer<LightDefinition> sysLightDefinitions;
rtDeclareVariable(int,    sysNumLights, , );
rtDeclareVariable(float,  sysEnvironmentRotation, , );

// Note that all light sampling routines return lightSample.direction and lightSample.distance in world space!

RT_CALLABLE_PROGRAM void sample_environment_light(float3 const& point, const float2 sample, LightSample& lightSample)
{
    LightDefinition light = sysLightDefinitions[lightSample.index];

    unitSquareToSphere(sample.x, sample.y, lightSample.direction, lightSample.pdf);

    // Environment lights do not set the light sample position!
    lightSample.distance = RT_DEFAULT_MAX; // Environment light.

    const float3 &R = lightSample.direction;
    // The seam u == 0.0 == 1.0 is in positive z-axis direction.
    // Compensate for the environment rotation done inside the direct lighting.
    const float u     = (atan2f(R.x, -R.z) + M_PIf) * 0.5f * M_1_PIf + sysEnvironmentRotation; // DAR FIXME Use a light.matrix to rotate the environment.
    const float theta = acosf(-R.y);     // theta == 0.0f is south pole, theta == M_PIf is north pole.
    const float v     = theta * M_1_PIf; // Texture is with origin at lower left, v == 0.0f is south pole.

    float3 texColor = make_float3(1.0f);
    if (light.environmentTextureID != RT_TEXTURE_ID_NULL)
        texColor = make_float3(optix::rtTex2D<float4>(light.environmentTextureID, u, v));
    lightSample.emission = light.emission * texColor;
}

RT_CALLABLE_PROGRAM void sample_directional_light(float3 const& point, const float2 sample, LightSample& lightSample)
{
    LightDefinition light = sysLightDefinitions[lightSample.index];

    lightSample.distance = 1e9;

    lightSample.position = point + lightSample.distance * light.direction;
    lightSample.direction = light.direction;
    lightSample.pdf = 1;

    lightSample.emission = light.emission;
}