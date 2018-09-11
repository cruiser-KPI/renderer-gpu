

#include <optix.h>
#include <optixu/optixu_math_namespace.h>

#include "../math/basic.h"
#include "../utils/config.h"
#include "../core/perraydata.h"
#include "../core/materialdata.h"


////////////////////////////////////////////////////////////
// Math helpers
////////////////////////////////////////////////////////////


RT_FUNCTION float3 world_to_local(const float3 &w, const State& state)
{
    return make_float3(dot(w, state.tangent), dot(w, state.bitangent), dot(w, state.normal));

}

RT_FUNCTION float3 local_to_world(const float3 &w, const State& state)
{
    return make_float3(state.tangent.x * w.x + state.bitangent.x * w.y + state.normal.x * w.z,
                       state.tangent.y * w.x + state.bitangent.y * w.y + state.normal.y * w.z,
                       state.tangent.z * w.x + state.bitangent.z * w.y + state.normal.z * w.z);
}

RT_FUNCTION float cos_theta(const float3 &w) { return w.z;}
RT_FUNCTION float cos2_theta(const float3 &w) { return w.z * w.z; }
RT_FUNCTION float abs_cos_theta(const float3 &w) { return fabsf(w.z); }
RT_FUNCTION float sin2_theta(const float3 &w) { return fmaxf(0.f, 1.f - cos2_theta(w)); }
RT_FUNCTION float sin_theta(const float3 &w) {return sqrtf(sin2_theta(w)); }
RT_FUNCTION float tan_theta(const float3 &w) {return sin_theta(w) / cos_theta(w); }
RT_FUNCTION float tan2_theta(const float3 &w) {return sin2_theta(w) / cos2_theta(w); }
RT_FUNCTION float cos_phi(const float3 &w) {
    float sinTheta = sin_theta(w);
    return (sinTheta == 0) ? 1 : clamp(w.x / sinTheta, -1.f, 1.f);
}
RT_FUNCTION float sin_phi(const float3 &w) {
    float sinTheta = sin_theta(w);
    return (sinTheta == 0) ? 0 : clamp(w.y / sinTheta, -1.f, 1.f);
}
RT_FUNCTION float cos2_phi(const float3 &w) { return cos_phi(w) * cos_phi(w); }
RT_FUNCTION float sin2_phi(const float3 &w) { return sin_phi(w) * sin_phi(w); }

RT_FUNCTION bool same_hemisphere(const float3 &w, const float3 &wp) {
    return w.z * wp.z > 0;
}

RT_FUNCTION float3 spherical_to_cartesian(float sinTheta, float cosTheta, float phi) {
    return make_float3(sinTheta * cosf(phi), sinTheta * sinf(phi), cosTheta);
}

RT_FUNCTION float3 Reflect(const float3 &wo, const float3 &n) {
    return -wo + 2 * dot(wo, n) * n;
}

RT_FUNCTION bool Refract(const float3 &wi, const float3 &n, float eta,
                    float3 *wt) {
    // Compute $\cos \theta_\roman{t}$ using Snell's law
    float cosThetaI = dot(n, wi);
    float sin2ThetaI = fmaxf(0.f, 1 - cosThetaI * cosThetaI);
    float sin2ThetaT = eta * eta * sin2ThetaI;

    // Handle total internal reflection for transmission
    if (sin2ThetaT >= 1) return false;
    float cosThetaT = std::sqrt(1 - sin2ThetaT);
    *wt = eta * -wi + (eta * cosThetaI - cosThetaT) * n;
    return true;
}


RT_FUNCTION float fresnel_dielectric(float cosThetaI, float etaI, float etaT) {
    cosThetaI = clamp(cosThetaI, -1.f, 1.f);
    // Potentially swap indices of refraction
    bool entering = cosThetaI > 0.f;
    if (!entering) {
        float tmp = etaI;
        etaI = etaT;
        etaT = tmp;
        cosThetaI = std::abs(cosThetaI);
    }

    // Compute _cosThetaT_ using Snell's law
    float sinThetaI = sqrtf(fmaxf(0.f, 1.f - cosThetaI * cosThetaI));
    float sinThetaT = etaI / etaT * sinThetaI;

    // Handle total internal reflection
    if (sinThetaT >= 1) return 1;
    float cosThetaT = sqrtf(fmaxf(0.f, 1.f - sinThetaT * sinThetaT));
    float Rparl = ((etaT * cosThetaI) - (etaI * cosThetaT)) /
        ((etaT * cosThetaI) + (etaI * cosThetaT));
    float Rperp = ((etaI * cosThetaI) - (etaT * cosThetaT)) /
        ((etaI * cosThetaI) + (etaT * cosThetaT));
    return (Rparl * Rparl + Rperp * Rperp) / 2;
}

// https://seblagarde.wordpress.com/2013/04/29/memo-on-fresnel-equations/
RT_FUNCTION float3 fresnel_conductor(float cosThetaI, const float3 &etai,
                                     const float3 &etat, const float3 &k) {
    cosThetaI = clamp(cosThetaI, -1.f, 1.f);
    float3 eta = etat / etai;
    float3 etak = k / etai;

    float cosThetaI2 = cosThetaI * cosThetaI;
    float sinThetaI2 = 1.f - cosThetaI2;
    float3 eta2 = eta * eta;
    float3 etak2 = etak * etak;

    float3 t0 = eta2 - etak2 - sinThetaI2;
    float3 a2plusb2 = sqrt(t0 * t0 + 4 * eta2 * etak2);
    float3 t1 = a2plusb2 + cosThetaI2;
    float3 a = sqrt(0.5f * (a2plusb2 + t0));
    float3 t2 = 2.f * cosThetaI * a;
    float3 Rs = (t1 - t2) / (t1 + t2);

    float3 t3 = cosThetaI2 * a2plusb2 + sinThetaI2 * sinThetaI2;
    float3 t4 = t2 * sinThetaI2;
    float3 Rp = Rs * (t3 - t4) / (t3 + t4);

    return 0.5 * (Rp + Rs);
}


RT_FUNCTION void roughness_to_alpha(float r, float aniso, float *alphax, float *alphay)
{
    r = fmaxf(r, 1e-3f);
    aniso = clamp(aniso, -0.99f, 0.99f);

    if (aniso < 0.0) {
        *alphax = r / (1.f + aniso);
        *alphay = r * (1.f + aniso);
    }
    else {
        *alphax = r * (1.f - aniso);
        *alphay = r / (1.f - aniso);
    }

    // TODO find out why we don't need to remap roughness
//    float x = log(roughness);
//    return 1.62142f + 0.819955f * x + 0.1734f * x * x + 0.0171201f * x * x * x +
//        0.000640711f * x * x * x * x;
}

RT_FUNCTION float ggx_aniso_d(const float3 &wh, float alphax, float alphay)
{
    float tan2Theta = tan2_theta(wh);
    if (isinf(tan2Theta)) return 0;
    const float cos4Theta = cos2_theta(wh) * cos2_theta(wh);
    float e = (cos2_phi(wh) / (alphax * alphax) + sin2_phi(wh) / (alphay * alphay)) * tan2Theta;
    return 1 / (M_PIf * alphax * alphay * cos4Theta * (1 + e) * (1 + e));
}

RT_FUNCTION float ggx_aniso_lambda(const float3 &w, float alphax, float alphay)
{
    float absTanTheta = fabsf(tan_theta(w));
    if (isinf(absTanTheta)) return 0;
    // Compute _alpha_ for direction _w_
    float alpha_hat = sqrtf(cos2_phi(w) * alphax * alphax + sin2_phi(w) * alphay * alphay);
    float alpha2Tan2Theta = (alpha_hat * absTanTheta) * (alpha_hat * absTanTheta);
    return (-1 + sqrtf(1.f + alpha2Tan2Theta)) / 2;
}

RT_FUNCTION float ggx_aniso_g(const float3 &wo, const float3 &wi, float alphax, float alphay)
{
    return 1.f / (1.f + ggx_aniso_lambda(wo, alphax, alphay) + ggx_aniso_lambda(wi, alphax, alphay));
}

RT_FUNCTION float ggx_aniso_pdf(const float3 &wo, const float3 &wh, float alphax, float alphay)
{
    float G1 = 1.f / (1.f + ggx_aniso_lambda(wo, alphax, alphay));
    return ggx_aniso_d(wh, alphax, alphay) * G1 * abs(dot(wo, wh)) / abs_cos_theta(wo);
}

RT_FUNCTION void ggx_aniso_sample11(float cosTheta, float U1, float U2,
                                    float *slope_x, float *slope_y) {
    // special case (normal incidence)
    if (cosTheta > .9999) {
        float r = sqrt(U1 / (1 - U1));
        float phi = 6.28318530718f * U2;
        *slope_x = r * cos(phi);
        *slope_y = r * sin(phi);
        return;
    }

    float sinTheta = sqrtf(fmaxf(0.f, 1.f - cosTheta * cosTheta));
    float tanTheta = sinTheta / cosTheta;
    float a = 1 / tanTheta;
    float G1 = 2 / (1 + sqrtf(1.f + 1.f / (a * a)));

    // sample slope_x
    float A = 2 * U1 / G1 - 1;
    float tmp = 1.f / (A * A - 1.f);
    if (tmp > 1e10) tmp = 1e10;
    float B = tanTheta;
    float D = sqrtf(fmaxf(B * B * tmp * tmp - (A * A - B * B) * tmp, .0f));
    float slope_x_1 = B * tmp - D;
    float slope_x_2 = B * tmp + D;
    *slope_x = (A < 0 || slope_x_2 > 1.f / tanTheta) ? slope_x_1 : slope_x_2;

    // sample slope_y
    float S;
    if (U2 > 0.5f) {
        S = 1.f;
        U2 = 2.f * (U2 - .5f);
    } else {
        S = -1.f;
        U2 = 2.f * (.5f - U2);
    }
    float z = (U2 * (U2 * (U2 * 0.27385f - 0.73369f) + 0.46341f)) /
            (U2 * (U2 * (U2 * 0.093073f + 0.309420f) - 1.000000f) + 0.597999f);
    *slope_y = S * z * std::sqrt(1.f + *slope_x * *slope_x);
}


RT_FUNCTION float3 ggx_aniso_sample_wh(const float3 &wo, const float2 &u, float alphax, float alphay)
{
    bool flip = wo.z < 0;
    float3 wi = (flip) ? -wo : wo;

    // 1. stretch wi
    float3 wiStretched = normalize(make_float3(alphax * wi.x, alphay * wi.y, wi.z));

    // 2. simulate P22_{wi}(x_slope, y_slope, 1, 1)
    float slope_x, slope_y;
    ggx_aniso_sample11(cos_theta(wiStretched), u.x, u.y, &slope_x, &slope_y);

    // 3. rotate
    float tmp = cos_phi(wiStretched) * slope_x - sin_phi(wiStretched) * slope_y;
    slope_y = sin_phi(wiStretched) * slope_x + cos_phi(wiStretched) * slope_y;
    slope_x = tmp;

    // 4. unstretch
    slope_x = alphax * slope_x;
    slope_y = alphay * slope_y;

    // 5. compute normal
    float3 wh = normalize(make_float3(-slope_x, -slope_y, 1.f));
    return (flip) ? -wh : wh;
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

    prd.f_over_pdf = parameters.albedo;
}


RT_CALLABLE_PROGRAM float4 eval_bsdf_diffuse_reflection(MaterialParameter const& parameters, State const& state,
    PerRayData const& prd, float3 const& wiL)
{
    const float3 f   = parameters.albedo * M_1_PIf;
    const float  pdf = fmaxf(0.0f, optix::dot(wiL, state.normal) * M_1_PIf);

    return make_float4(f, pdf);
}


////////////////////////////////////////////////////////////
// Glossy bsdf (with Fresnel)
////////////////////////////////////////////////////////////


RT_CALLABLE_PROGRAM float4 eval_bsdf_glossy(MaterialParameter const& parameters, State const& state, PerRayData const& prd, float3 const& wiL)
{
    float3 wo = world_to_local(prd.wo, state), wi = world_to_local(wiL, state);
    float cosThetaO = abs_cos_theta(wo), cosThetaI = abs_cos_theta(wi);
    float3 wh = wi + wo;
    // Handle degenerate cases for microfacet reflection
    if ((cosThetaI == 0 || cosThetaO == 0) || (wh.x == 0 && wh.y == 0 && wh.z == 0) ||
        !same_hemisphere(wo, wi))
        return make_float4(0.f);
    wh = normalize(wh);
//    float F = fresnel_dielectric(dot(wi, wh), 1, parameters.ior);

    float alphax, alphay;
    roughness_to_alpha(parameters.roughness, parameters.anisotropy, &alphax, &alphay);
    float3 f = parameters.albedo * ggx_aniso_d(wh, alphax, alphay) *
        ggx_aniso_g(wo, wi, alphax, alphay) / (4 * cosThetaI * cosThetaO);
    float pdf = ggx_aniso_pdf(wo, wh, alphax, alphay) / (4 * dot(wo, wh));

    return make_float4(f, pdf);
}

RT_CALLABLE_PROGRAM void sample_bsdf_glossy(MaterialParameter const& parameters, State const& state, PerRayData& prd)
{
    float3 wo = world_to_local(prd.wo, state);
    if (wo.z == 0) {
        prd.flags |= FLAG_TERMINATE;
        return;
    }

    float alphax, alphay;
    roughness_to_alpha(parameters.roughness, parameters.anisotropy, &alphax, &alphay);
    float3 wh = ggx_aniso_sample_wh(wo, rng2(prd.seed), alphax, alphay);
    float3 wi = normalize(Reflect(wo, wh));

    if (!same_hemisphere(wo, wi)) {
        prd.flags |= FLAG_TERMINATE;
        return;
    }

    prd.wi = local_to_world(wi, state);
    float4 bsdf_val = eval_bsdf_glossy(parameters, state, prd, prd.wi);
    prd.pdf = bsdf_val.w;
    prd.f_over_pdf = make_float3(bsdf_val) * fabsf(dot(prd.wi, state.normal)) / prd.pdf;

    if (prd.pdf <= 0.0f || dot(prd.wi, state.geoNormal) <= 0.0f) {
        prd.flags |= FLAG_TERMINATE;
        return;
    }


}

////////////////////////////////////////////////////////////
// Refraction bsdf (with Fresnel)
////////////////////////////////////////////////////////////

RT_CALLABLE_PROGRAM float4 eval_bsdf_refraction(MaterialParameter const& parameters, State const& state, PerRayData const& prd, float3 const& wiL)
{
    float3 wo = world_to_local(prd.wo, state), wi = world_to_local(wiL, state);
    float cosThetaO = abs_cos_theta(wo), cosThetaI = abs_cos_theta(wi);

    if ((cosThetaI == 0 || cosThetaO == 0) || same_hemisphere(wo, wi))
        return make_float4(0.f);

    float eta = cos_theta(wo) > 0 ? (parameters.ior / 1.f) : (1.f / parameters.ior);
    float3 wh = normalize(wo + wi * eta);
    if (wh.z < 0) wh = -wh;

    // TODO think what to do with fresnel in microfacet models
//    float F = fresnel_dielectric(dot(wo, wh), 1, parameters.ior);
    float sqrtDenom = dot(wo, wh) + eta * dot(wi, wh);

    float alphax, alphay;
    roughness_to_alpha(parameters.roughness, parameters.anisotropy, &alphax, &alphay);
    float3 f =  parameters.albedo * fabsf(ggx_aniso_d(wh, alphax, alphay) *
        ggx_aniso_g(wo, wi, alphax, alphay) * eta * eta * fabsf(dot(wi, wh)) * fabsf(dot(wo, wh))
        / (cosThetaI * cosThetaO * sqrtDenom * sqrtDenom));

    float dwh_dwi = fabsf((eta * eta * dot(wi, wh)) / (sqrtDenom * sqrtDenom));
    float pdf = ggx_aniso_pdf(wo, wh, alphax, alphay) * dwh_dwi;

    return make_float4(f, pdf);

}

RT_CALLABLE_PROGRAM void sample_bsdf_refraction(MaterialParameter const& parameters, State const& state, PerRayData& prd)
{
    float3 wo = world_to_local(prd.wo, state);
    if (wo.z == 0) {
        prd.flags |= FLAG_TERMINATE;
        return;
    }

    float alphax, alphay;
    roughness_to_alpha(parameters.roughness, parameters.anisotropy, &alphax, &alphay);
    float3 wh = ggx_aniso_sample_wh(wo, rng2(prd.seed), alphax, alphay);
    float eta = cos_theta(wo) > 0 ? (1.f / parameters.ior) : (parameters.ior / 1.f);
    float3 wi;
    if (!Refract(wo, wh, eta, &wi)) {
        prd.flags |= FLAG_TERMINATE;
        return;
    }

    prd.wi = local_to_world(wi, state);
    float4 bsdf_val = eval_bsdf_refraction(parameters, state, prd, prd.wi);
    prd.pdf = bsdf_val.w;
    prd.f_over_pdf = make_float3(bsdf_val) * fabsf(dot(prd.wi, state.normal)) / prd.pdf;
}


RT_CALLABLE_PROGRAM float4 eval_bsdf_glass(MaterialParameter const& parameters, State const& state, PerRayData const& prd, float3 const& wiL)
{
    float F = fresnel_dielectric(dot(wiL, state.normal), 1, parameters.ior);
    float4 res = make_float4(0.f);
    unsigned int seed = prd.seed;
    if (rng(seed) < F){
        res = eval_bsdf_glossy(parameters, state, prd, wiL);
        res = make_float4(make_float3(res) * F, res.w / F);
    }
    else {
        res = eval_bsdf_refraction(parameters, state, prd, wiL);
        F = 1 - F;
        res = make_float4(make_float3(res) * F, res.w / F);
    }
    return res;
}


RT_CALLABLE_PROGRAM void sample_bsdf_glass(MaterialParameter const& parameters, State const& state, PerRayData& prd)
{
    float F = fresnel_dielectric(dot(prd.wo, state.normal), 1, parameters.ior);
    if (rng(prd.seed) < F){
        sample_bsdf_glossy(parameters, state, prd);
        prd.pdf /= F;
        prd.f_over_pdf *= F;
    }
    else {
        sample_bsdf_refraction(parameters, state, prd);
        prd.pdf /= (1 - F);
        prd.f_over_pdf *= (1 - F);
    }
}