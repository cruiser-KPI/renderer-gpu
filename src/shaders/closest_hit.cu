#include <optix.h>
#include <optixu/optixu_math_namespace.h>
#include <optix_world.h>
using namespace optix;

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
rtDeclareVariable(optix::float3, varTangent,   attribute TANGENT, );
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
    if (sysNumLights == 0)
        return;

    MaterialParameter parameters = sysMaterialParameters[materialIndex];
    float mixFactor = 1.f;
    while (parameters.indexBSDF == MaterialType::MIX) // handle mix materials
    {
        // TODO fix mix factor (using ior for mix factor)
        // ior is used here for mix factor

        parameters.ior = clamp(parameters.ior, 0.01f, 0.99f);

        if (rng(thePrd.seed) < parameters.ior) {
            mixFactor = parameters.ior;
            parameters = sysMaterialParameters[materialIndex + 1];
        }
        else {
            mixFactor = (1 - parameters.ior);
            parameters = sysMaterialParameters[materialIndex + 2];
        }
    }

    State state; // All in world space coordinates!
    state.geoNormal = normalize(rtTransformNormal(RT_OBJECT_TO_WORLD, varGeoNormal));
    state.normal = normalize(rtTransformNormal(RT_OBJECT_TO_WORLD, varNormal));
    if (parameters.rotation != 0)
        // rotate tangent for anisotropic materials
        state.tangent = make_float3(normalize(
            Matrix4x4::rotate(parameters.rotation * 2 * M_PIf, state.normal) * make_float4(RT_OBJECT_TO_WORLD * varTangent)));
    else
        state.tangent = normalize(RT_OBJECT_TO_WORLD * varTangent);
    state.texcoord  = varTexCoord;

    thePrd.flags |= (0.0f <= optix::dot(thePrd.wo, state.geoNormal)) ? FLAG_FRONTFACE : 0;

    if ((thePrd.flags & FLAG_FRONTFACE) == 0)
    {
        state.geoNormal = -state.geoNormal;
        state.normal = -state.normal;
    }
    state.bitangent = cross(state.normal, state.tangent);

    thePrd.pos = theRay.origin + theRay.direction * theIntersectionDistance;
    thePrd.distance = theIntersectionDistance;

    thePrd.radiance = make_float3(0.0f);
    if (parameters.textureID != RT_TEXTURE_ID_NULL)
    {
        const float3 texColor = make_float3(optix::rtTex2D<float4>(parameters.textureID,
                                                                   parameters.textureScale * state.texcoord.x, parameters.textureScale * state.texcoord.y));
        parameters.albedo *= texColor;
    }
    thePrd.f_over_pdf = make_float3(0.0f);
    thePrd.pdf = 0.0f;
    thePrd.flags = parameters.flags | FLAG_PATH;

    // --- importance sample light source
    const float2 sample = rng2(thePrd.seed);

    LightSample lightSample;
    lightSample.index = optix::clamp(static_cast<int>(
                                         floorf(rng(thePrd.seed) * sysNumLights)), 0, sysNumLights - 1);


    const LightType lightType = sysLightDefinitions[lightSample.index].type;
    sysSampleLight[lightType](thePrd.pos, sample, lightSample);
    if (0.0f < lightSample.pdf) {
        // handle delta lights
        float4 bsdf_pdf = sysEvalBSDF[parameters.indexBSDF](parameters, state, thePrd, lightSample.direction);

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
                bsdf_pdf = make_float4(make_float3(bsdf_pdf) * mixFactor, bsdf_pdf.w / mixFactor);

                // don't importance sample delta lights
                if (lightType == DIRECTIONAL || lightType == POINT) {
                    // TODO solve dark spot problem in transparent material for directional lights
                    thePrd.radiance += make_float3(bsdf_pdf) * lightSample.emission * float(sysNumLights) *
                        (dot(lightSample.direction, state.normal) / lightSample.pdf);
                }
                else {
                    const float misWeight = powerHeuristic(lightSample.pdf, bsdf_pdf.w);

                    thePrd.radiance += make_float3(bsdf_pdf) * lightSample.emission * float(sysNumLights) *
                        (misWeight * dot(lightSample.direction, state.normal) / lightSample.pdf);
                }
            }
        }

    }

    // --- sample BSDF to find next ray direction
    sysSampleBSDF[parameters.indexBSDF](parameters, state, thePrd);
    thePrd.pdf /= mixFactor;
    thePrd.f_over_pdf *= mixFactor;

}