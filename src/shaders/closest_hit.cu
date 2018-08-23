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

// Helper functions for sampling a cosine weighted hemisphere distribution as needed for the Lambert shading model.

RT_FUNCTION void alignVector(float3 const &axis, float3 &w)
{
    // Align w with axis.
    const float s = copysign(1.0f, axis.z);
    w.z *= s;
    const float3 h = make_float3(axis.x, axis.y, axis.z + s);
    const float k = optix::dot(w, h) / (1.0f + fabsf(axis.z));
    w = k * h - w;
}

RT_FUNCTION void unitSquareToCosineHemisphere(const float2 sample, float3 const &axis, float3 &w, float &pdf)
{
    // Choose a point on the local hemisphere coordinates about +z.
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

RT_PROGRAM void closest_hit()
{
    State state; // All in world space coordinates!
    state.geoNormal = optix::normalize(rtTransformNormal(RT_OBJECT_TO_WORLD, varGeoNormal));
    state.normal = optix::normalize(rtTransformNormal(RT_OBJECT_TO_WORLD, varNormal));
    state.texcoord  = varTexCoord;

    thePrd.pos = theRay.origin
        + theRay.direction * theIntersectionDistance; // Advance the path to the hit position in world coordinates.
    thePrd.distance =
        theIntersectionDistance; // Return the current path segment distance, needed for absorption calculations in the integrator.

    // Explicitly include edge-on cases as frontface condition!
    // Keeps the material stack from overflowing at silhouttes.
    // Prevents that silhouettes of thin-walled materials use the backface material.
    // Using the true geometry normal attribute as originally defined on the frontface!
    thePrd.flags |= (0.0f <= optix::dot(thePrd.wo, state.geoNormal)) ? FLAG_FRONTFACE : 0;

    if ((thePrd.flags & FLAG_FRONTFACE) == 0) // Looking at the backface?
    {
        // Means geometric normal and shading normal are always defined on the side currently looked at.
        // This gives the backfaces of opaque BSDFs a defined result.
        state.geoNormal = -state.geoNormal;
        state.normal = -state.normal;
        // Do not recalculate the frontface condition!
    }

    // A material system with support for arbitrary mesh lights would evaluate its emission here.
    // But since only parallelogram area lights are supported, those get a dedicated closest hit program to simplify this demo.
    thePrd.radiance = make_float3(0.0f);

    MaterialParameter parameters = sysMaterialParameters[materialIndex];

    if (parameters.textureID != RT_TEXTURE_ID_NULL)
    {
        const float3 texColor = make_float3(optix::rtTex2D<float4>(parameters.textureID,
            parameters.textureScale * state.texcoord.x, parameters.textureScale * state.texcoord.y));

        // Modulate the incoming color with the texture.
        parameters.albedo *= texColor;               // linear color, resp. if the texture has been uint8 and readmode set to use sRGB, then sRGB.
        //parameters.albedo *= powf(texColor, 2.2f); // sRGB gamma correction done manually.
    }

    // Start fresh with the next BSDF sample.  (Either of these values remaining zero is an end-of-path condition.)
    thePrd.f_over_pdf = make_float3(0.0f);
    thePrd.pdf = 0.0f;

    // Only the last diffuse hit is tracked for multiple importance sampling of implicit light hits.
    thePrd.flags = (thePrd.flags & ~FLAG_DIFFUSE)
        | parameters.flags; // FLAG_THINWALLED can be set directly from the material parameters.

    sysSampleBSDF[parameters.indexBSDF](parameters, state, thePrd);

    // Direct lighting if the sampled BSDF was diffuse and any light is in the scene.
    if ((thePrd.flags & FLAG_DIFFUSE) && 0 < sysNumLights) {
        const float2
            sample = rng2(thePrd.seed); // Use higher dimension samples for the position. (Irrelevant for the LCG).

        LightSample
        lightSample; // Sample one of many lights.

        // The caller picks the light to sample. Make sure the index stays in the bounds of the sysLightDefinitions array.
        lightSample.index =
            optix::clamp(static_cast<int>(floorf(rng(thePrd.seed) * sysNumLights)), 0, sysNumLights - 1);

        const LightType lightType = sysLightDefinitions[lightSample.index].type;

        sysSampleLight[lightType](thePrd.pos, sample, lightSample);

        if (0.0f < lightSample.pdf) // Useful light sample?
        {
            // Evaluate the BSDF in the light sample direction. Normally cheaper than shooting rays.
            // Returns BSDF f in .xyz and the BSDF pdf in .w
            const float4 bsdf_pdf = sysEvalBSDF[parameters.indexBSDF](parameters, state, thePrd, lightSample.direction);

            if (0.0f < bsdf_pdf.w && isNotNull(make_float3(bsdf_pdf))) {
                // Do the visibility check of the light sample.
                PerRayData_shadow prdShadow;

                prdShadow.visible = true; // Initialize for miss.

                // Note that the sysSceneEpsilon is applied on both sides of the shadow ray [t_min, t_max] interval
                // to prevent self intersections with the actual light geometry in the scene!
                optix::Ray ray = optix::make_Ray(thePrd.pos,
                                                 lightSample.direction,
                                                 1,
                                                 sysSceneEpsilon,
                                                 lightSample.distance - sysSceneEpsilon); // Shadow ray.
                rtTrace(sysTopObject, ray, prdShadow);

                if (prdShadow.visible) {
                    if (thePrd.flags
                        & FLAG_VOLUME) // Supporting nested materials includes having lights inside a volume.
                    {
                        // Calculate the transmittance along the light sample's distance in case it's inside a volume.
                        // The light must be in the same volume or it would have been shadowed!
                        lightSample.emission *= expf(-lightSample.distance * thePrd.extinction);
                    }

                    const float misWeight = powerHeuristic(lightSample.pdf, bsdf_pdf.w);

                    thePrd.radiance += make_float3(bsdf_pdf) * lightSample.emission * float(sysNumLights) *
                        (misWeight * optix::dot(lightSample.direction, state.normal) / lightSample.pdf);
                }
            }
        }
    }
}