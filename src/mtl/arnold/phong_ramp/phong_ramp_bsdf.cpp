#include "phong_ramp_bsdf.h"

// Based on the matching BSDFs from Cycles 

struct PhongRampBSDF
{
    /* parameters */
    AtVector N;
    float    exponent;
    AtArray* colors;

    /* set in bsdf_init */
    AtVector Ng, Ns, Rd;
};

static AtRGB PhongRampGetColor(const AtArray* colors, float pos)
{
    const int MAXCOLORS = 8;
    const float npos = pos * (float)(MAXCOLORS - 1);
    const int ipos   = (int)npos;
    if (ipos < 0)
        return AiArrayGetRGB(colors, 0);
    if (ipos >= MAXCOLORS - 1)
        return AiArrayGetRGB(colors, MAXCOLORS - 1);
    const float offset = npos - (float)ipos;
    return AiArrayGetRGB(colors, ipos) * (1.0f - offset) +
           AiArrayGetRGB(colors, ipos + 1) * offset;
}

static float PhongExponentToRoughness(float exponent)
{
    return sqrtf(1.0f / ((exponent + 2.0f) * 0.5f));
}

AI_BSDF_EXPORT_METHODS(PhongRampBSDFMtd);

bsdf_init
{
    PhongRampBSDF* data = (PhongRampBSDF*)AiBSDFGetData(bsdf);

    data->Ns = (sg->Ngf == sg->Ng) ? sg->Ns : -sg->Ns;
    data->Ng = sg->Ngf;
    data->Rd = sg->Rd;

    static const AtBSDFLobeInfo lobe_info[1] = {
        { AI_RAY_SPECULAR_REFLECT, 0, AtString() }
    };
    AiBSDFInitLobes(bsdf, lobe_info, 1);
    AiBSDFInitNormal(bsdf, data->N, true);
}

bsdf_sample
{
    PhongRampBSDF* data = (PhongRampBSDF*)AiBSDFGetData(bsdf);

    const float cosNI = AiV3Dot(data->N, data->Rd * -1.0f);  // wi = -Rd
    if (cosNI <= 0.0f)
        return AI_BSDF_LOBE_MASK_NONE;

    const float m_exp = data->exponent;

    // reflect the view vector around N to get the mirror direction R
    AtVector wi  = data->Rd * -1.0f;
    AtVector R   = 2.0f * cosNI * data->N - wi;

    // build local frame around R for Phong sampling
    AtVector T, B;
    AiV3BuildLocalFrame(T, B, R);

    // sample Phong lobe: cosTheta^(1/(n+1)) 
    const float phi      = 2.0f * AI_PI * rnd.x;
    const float cosTheta = powf(rnd.y, 1.0f / (m_exp + 1.0f));
    const float sinTheta = sqrtf(1.0f - cosTheta * cosTheta);

    AtVector wo = sinTheta * cosf(phi) * T +
                  sinTheta * sinf(phi) * B +
                  cosTheta * R;

    // discard rays below geometric surface
    if (!(AiV3Dot(wo, data->Ng) > 0.0f))
        return AI_BSDF_LOBE_MASK_NONE;

    const float cosNO = AiV3Dot(data->N, wo);
    if (cosNO <= 0.0f)
        return AI_BSDF_LOBE_MASK_NONE;

    const float cosp   = powf(cosTheta, m_exp);
    const float common = 0.5f * AI_ONEOVERPI * cosp;
    const float pdf    = (m_exp + 1.0f) * common;
    const float eval   = cosNO * (m_exp + 2.0f) * common;

    const AtRGB rampColor = PhongRampGetColor(data->colors, cosp);
    const float roughness = PhongExponentToRoughness(m_exp);

    out_wi         = AtVectorDv(wo);
    out_lobe_index = 0;
    // weight = eval / pdf = cosNO * (n+2) / (n+1)
    out_lobes[0]   = AtBSDFLobeSample(rampColor * (eval / pdf), roughness, pdf);
    return lobe_mask;
}

bsdf_eval
{
    PhongRampBSDF* data = (PhongRampBSDF*)AiBSDFGetData(bsdf);

    const float cosNI = AiV3Dot(data->N, data->Rd * -1.0f);
    const float cosNO = AiV3Dot(data->N, wi);

    if (cosNI <= 0.0f || cosNO <= 0.0f)
        return AI_BSDF_LOBE_MASK_NONE;

    const float m_exp = data->exponent;

    // reflect incoming view vector
    AtVector view = data->Rd * -1.0f;
    AtVector R = 2.0f * cosNI * data->N - view;
    const float cosRO = AiV3Dot(R, wi);

    if (cosRO <= 0.0f)
        return AI_BSDF_LOBE_MASK_NONE;

    const float cosp   = powf(cosRO, m_exp);
    const float common = 0.5f * AI_ONEOVERPI * cosp;
    const float pdf    = (m_exp + 1.0f) * common;
    const float eval   = cosNO * (m_exp + 2.0f) * common;

    const AtRGB rampColor = PhongRampGetColor(data->colors, cosp);
    const float roughness = PhongExponentToRoughness(m_exp);

    out_lobes[0] = AtBSDFLobeSample(rampColor * eval, roughness, pdf);
    return lobe_mask;
}

AtBSDF* PhongRampBSDFCreate(const AtShaderGlobals* sg,
                             const AtVector& N,
                             float exponent,
                             AtRGB colorA, AtRGB colorB, AtRGB colorC, AtRGB colorD,
                             AtRGB colorE, AtRGB colorF, AtRGB colorG, AtRGB colorH)
{
    AtBSDF* bsdf = AiBSDF(sg, AtRGB(1.0f), PhongRampBSDFMtd, sizeof(PhongRampBSDF));
    PhongRampBSDF* data = (PhongRampBSDF*)AiBSDFGetData(bsdf);
    data->N        = N;
    data->exponent = AiMax(exponent, 0.0f);
    data->colors   = AiArray(8, 1, AI_TYPE_RGB,
                             colorA, colorB, colorC, colorD,
                             colorE, colorF, colorG, colorH);
    return bsdf;
}