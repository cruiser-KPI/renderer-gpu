

#include <optix.h>
#include <optixu/optixu_math_namespace.h>

#include "../utils/config.h"
#include "../core/perraydata.h"
#include "../core/materialdata.h"

////////////////////////////////////////////////////////////
// Helper functions
////////////////////////////////////////////////////////////

// This function evaluates a Fresnel dielectric function when the transmitting cosine ("cost")
// is unknown and the incident index of refraction is assumed to be 1.0f.
// \param et     The transmitted index of refraction.
// \param costIn The cosine of the angle between the incident direction and normal direction.
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

////////////////////////////////////////////////////////////
// Diffuse BSDF (Lambertian)
////////////////////////////////////////////////////////////

RT_CALLABLE_PROGRAM void sample_bsdf_diffuse_reflection(MaterialParameter const& parameters, State const& state, PerRayData& prd)
{
    // Cosine weighted hemisphere sampling for Lambert material.
    unitSquareToCosineHemisphere(rng2(prd.seed), state.normal, prd.wi, prd.pdf);

    if (prd.pdf <= 0.0f || optix::dot(prd.wi, state.geoNormal) <= 0.0f)
    {
        prd.flags |= FLAG_TERMINATE;
        return;
    }

    // This would be the universal implementation for an arbitrary sampling of a diffuse surface.
    // prd.f_over_pdf = parameters.albedo * (M_1_PIf * fabsf(optix::dot(prd.wi, state.normal)) / prd.pdf);

    // PERF Since the cosine-weighted hemisphere distribution is a perfect importance-sampling of the Lambert material,
    // the whole term ((M_1_PIf * fabsf(optix::dot(prd.wi, state.normal)) / prd.pdf) is always 1.0f here!
    prd.f_over_pdf = parameters.albedo;

    prd.flags |= FLAG_DIFFUSE; // Direct lighting will be done with multiple importance sampling.
}

// The parameter wiL is the lightSample.direction (direct lighting), not the next ray segment's direction prd.wi (indirect lighting).
RT_CALLABLE_PROGRAM float4 eval_bsdf_diffuse_reflection(MaterialParameter const& parameters, State const& state, PerRayData const& prd, float3 const& wiL)
{
const float3 f   = parameters.albedo * M_1_PIf;
const float  pdf = fmaxf(0.0f, optix::dot(wiL, state.normal) * M_1_PIf);

return make_float4(f, pdf);
}

////////////////////////////////////////////////////////////
// Specular BSDF
////////////////////////////////////////////////////////////

RT_CALLABLE_PROGRAM void sample_bsdf_specular_reflection(MaterialParameter const& parameters, State const& state, PerRayData& prd)
{
    prd.wi = optix::reflect(-prd.wo, state.normal);

    if (optix::dot(prd.wi, state.geoNormal) <= 0.0f) // Do not sample opaque materials below the geometric surface.
    {
        prd.flags |= FLAG_TERMINATE;
        return;
    }

    prd.f_over_pdf = parameters.albedo;
    prd.pdf        = 1.0f;
}

// This is actually never reached, because the FLAG_DIFFUSE flag is not set when a specular BSDF is has been sampled.
RT_CALLABLE_PROGRAM float4 eval_bsdf_specular_reflection(MaterialParameter const& parameters, State const& state, PerRayData const& prd, float3 const& wiL)
{
return make_float4(0.0f);
}

////////////////////////////////////////////////////////////
// Specular reflection and transmission BSDF (with Fresnel)
////////////////////////////////////////////////////////////

RT_CALLABLE_PROGRAM void sample_bsdf_specular_reflection_transmission(MaterialParameter const& parameters, State const& state, PerRayData& prd)
{
    // Return the current material's absorption coefficient and ior to the integrator to be able to support nested materials.
    prd.absorption_ior = make_float4(parameters.absorption, parameters.ior);

    // Need to figure out here which index of refraction to use if the ray is already inside some refractive medium.
    // This needs to happen with the original FLAG_FRONTFACE condition to find out from which side of the geometry we're looking!
    // ior.xy are the current volume's IOR and the surrounding volume's IOR.
    // Thin-walled materials have no volume, always use the frontface eta for them!
    const float eta = (prd.flags & (FLAG_FRONTFACE | FLAG_THINWALLED))
                      ? prd.absorption_ior.w / prd.ior.x
                      : prd.ior.y / prd.absorption_ior.w;

    const float3 R = optix::reflect(-prd.wo, state.normal);

    float reflective = 1.0f;

    if (optix::refract(prd.wi, -prd.wo, state.normal, eta))
    {
        if (prd.flags & FLAG_THINWALLED)
        {
            prd.wi = -prd.wo; // Straight through, no volume.
        }
        // Total internal reflection will leave this reflection probability at 1.0f.
        reflective = evaluateFresnelDielectric(eta, optix::dot(prd.wo, state.normal));
    }

    const float pseudo = rng(prd.seed);
    if (pseudo < reflective)
    {
        prd.wi = R; // Fresnel reflection or total internal reflection.
    }
    else if (!(prd.flags & FLAG_THINWALLED)) // Only non-thinwalled materials have a volume and transmission events.
    {
        prd.flags |= FLAG_TRANSMISSION;
    }

    // No Fresnel factor here. The probability to pick one or the other side took care of that.
    prd.f_over_pdf = parameters.albedo;
    prd.pdf        = 1.0f;
}

RT_CALLABLE_PROGRAM float4 eval_bsdf_specular_reflection_transmission(MaterialParameter const& parameters, State const& state, PerRayData const& prd, float3 const& wiL)
{
  return make_float4(0.0f);
}