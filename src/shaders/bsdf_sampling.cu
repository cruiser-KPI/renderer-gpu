

#include <optix.h>
#include <optixu/optixu_math_namespace.h>

#include "../math/basic.h"
#include "../utils/config.h"
#include "../core/perraydata.h"
#include "../core/materialdata.h"


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


RT_CALLABLE_PROGRAM float4 eval_bsdf_diffuse_reflection(MaterialParameter const& parameters, State const& state,
    PerRayData const& prd, float3 const& wiL)
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

    const float eta = (prd.flags & FLAG_FRONTFACE)
                      ? prd.absorption_ior.w / prd.ior.x
                      : prd.ior.y / prd.absorption_ior.w;

    const float3 R = optix::reflect(-prd.wo, state.normal);

    float reflective = 1.0f;

    if (optix::refract(prd.wi, -prd.wo, state.normal, eta))
    {
        prd.wi = -prd.wo;
        // Total internal reflection will leave this reflection probability at 1.0f.
        reflective = evaluateFresnelDielectric(eta, optix::dot(prd.wo, state.normal));
    }

    const float pseudo = rng(prd.seed);
    if (pseudo < reflective)
    {
        prd.wi = R; // Fresnel reflection or total internal reflection.
    }

    // No Fresnel factor here. The probability to pick one or the other side took care of that.
    prd.f_over_pdf = parameters.albedo;
    prd.pdf        = 1.0f;
}

RT_CALLABLE_PROGRAM float4 eval_bsdf_specular_reflection_transmission(MaterialParameter const& parameters, State const& state, PerRayData const& prd, float3 const& wiL)
{
  return make_float4(0.0f);
}