#include <optix.h>
#include <optixu/optixu_math_namespace.h>

#include "../utils/config.h"
#include "../core/perraydata.h"
#include "../core/materialdata.h"
#include "../core/lightdata.h"
#include "../math/basic.h"

// Context global variables provided by the renderer system.
rtDeclareVariable(rtObject, sysTopObject, , );
rtDeclareVariable(float, sysSceneEpsilon, , );

// Semantic variables.
rtDeclareVariable(optix::Ray, theRay, rtCurrentRay, );
rtDeclareVariable(float, theIntersectionDistance, rtIntersectionDistance,);
rtDeclareVariable(PerRayData, thePrd, rtPayload, );

// Attributes.
rtDeclareVariable(optix::float3, varGeoNormal, attribute GEO_NORMAL, );
//rtDeclareVariable(optix::float3, varTangent,   attribute TANGENT, );
rtDeclareVariable(optix::float3, varNormal, attribute NORMAL, );
rtDeclareVariable(optix::float3, varTexCoord,  attribute TEXCOORD, );

// Material parameter definition.
rtBuffer<MaterialParameter> sysMaterialParameters; // Context global buffer with an array of structures of MaterialParameter.
rtDeclareVariable(int, materialIndex, , ); // Per Material index into the sysMaterialParameters array.

rtBuffer<LightDefinition> sysLightDefinitions;
rtDeclareVariable(int, sysNumLights, , );     // PERF Used many times and faster to read than sysLightDefinitions.size().

rtBuffer< rtCallableProgramId<void(float3 const& point, const float2 sample, LightSample& lightSample)> >
    sysSampleLight;

rtBuffer<rtCallableProgramId<void(MaterialParameter const &parameters, State const &state, PerRayData &prd)>>
    sysSampleBSDF;

rtBuffer<rtCallableProgramId<float4(MaterialParameter const& parameters, State const &state, PerRayData const& prd,
    float3 const &wiL)> > sysEvalBSDF;

RT_PROGRAM void closest_hit()
{
    State state; // All in world space coordinates!
    state.geoNormal = optix::normalize(rtTransformNormal(RT_OBJECT_TO_WORLD, varGeoNormal));
    state.normal = optix::normalize(rtTransformNormal(RT_OBJECT_TO_WORLD, varNormal));
    state.texcoord  = varTexCoord;

    thePrd.pos = theRay.origin + theRay.direction * theIntersectionDistance;
    thePrd.distance = theIntersectionDistance;

    thePrd.flags |= (0.0f <= optix::dot(thePrd.wo, state.geoNormal)) ? FLAG_FRONTFACE : 0;

    if ((thePrd.flags & FLAG_FRONTFACE) == 0)
    {
        state.geoNormal = -state.geoNormal;
        state.normal = -state.normal;
    }

    thePrd.radiance = make_float3(0.0f);
    MaterialParameter parameters = sysMaterialParameters[materialIndex];
    if (parameters.textureID != RT_TEXTURE_ID_NULL)
    {
        const float3 texColor = make_float3(optix::rtTex2D<float4>(parameters.textureID,
            parameters.textureScale * state.texcoord.x, parameters.textureScale * state.texcoord.y));
        parameters.albedo *= texColor;
    }

    thePrd.f_over_pdf = make_float3(0.0f);
    thePrd.pdf = 0.0f;
    thePrd.flags = (thePrd.flags & ~FLAG_DIFFUSE) | parameters.flags;

    sysSampleBSDF[parameters.indexBSDF](parameters, state, thePrd);

    if ((thePrd.flags & FLAG_DIFFUSE) && 0 < sysNumLights) {
        const float2 sample = rng2(thePrd.seed);

        LightSample lightSample;
        lightSample.index = optix::clamp(static_cast<int>(
            floorf(rng(thePrd.seed) * sysNumLights)), 0, sysNumLights - 1);

        const LightType lightType = sysLightDefinitions[lightSample.index].type;
        sysSampleLight[lightType](thePrd.pos, sample, lightSample);
        if (0.0f < lightSample.pdf)
        {
            const float4 bsdf_pdf = sysEvalBSDF[parameters.indexBSDF](parameters, state, thePrd, lightSample.direction);

            if (0.0f < bsdf_pdf.w && isNotNull(make_float3(bsdf_pdf))) {

                PerRayData_shadow prdShadow;
                prdShadow.visible = true;

                optix::Ray ray = optix::make_Ray(thePrd.pos,
                                                 lightSample.direction,
                                                 1,
                                                 sysSceneEpsilon,
                                                 lightSample.distance - sysSceneEpsilon); // Shadow ray.
                rtTrace(sysTopObject, ray, prdShadow);

                if (prdShadow.visible) {
                    const float misWeight = powerHeuristic(lightSample.pdf, bsdf_pdf.w);

                    thePrd.radiance += make_float3(bsdf_pdf) * lightSample.emission * float(sysNumLights) *
                        (misWeight * optix::dot(lightSample.direction, state.normal) / lightSample.pdf);
                }
            }
        }
    }
}