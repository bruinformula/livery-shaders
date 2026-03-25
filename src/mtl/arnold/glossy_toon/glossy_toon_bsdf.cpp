#include "glossy_toon_bsdf.h"

#include <ai_color.h>
#include <ai_constants.h>
#include <ai_math.h>
#include <ai_ray.h>
#include <ai_shader_bsdf.h>
#include <ai_shader_util.h>
#include <ai_shaderglobals.h>
#include <ai_string.h>
#include <ai_vector.h>
#include <math.h>

struct AtBSDF;

// Based on the Toon BSDFs from Blender/Cycles

static void toon_setup_params(float& size, float& smooth)
{
    size   = AiClamp(size,   1e-5f, 1.0f) * AI_PIOVER2;
    smooth = AiClamp(smooth, 0.0f,  1.0f) * AI_PIOVER2;
}

static float toon_get_intensity(float max_angle, float smooth, float angle)
{
    if (angle < max_angle)
        return 1.0f;
    if (smooth > 0.0f && angle < max_angle + smooth)
        return 1.0f - (angle - max_angle) / smooth;
    return 0.0f;
}

static float toon_sample_angle(float max_angle, float smooth)
{
    return AiMin(max_angle + smooth, AI_PIOVER2);
}

static float one_minus_cos(float x)
{
    return 1.0f - cosf(x);
}

static AtVector sample_uniform_cone(const AtVector& N,
                                    float omc,
                                    float rnd_x, float rnd_y,
                                    float& cosNI_out,
                                    float& pdf_out)
{
    cosNI_out = 1.0f - rnd_x * omc;
    const float sin_theta = sqrtf(AiMax(0.0f, 1.0f - cosNI_out * cosNI_out));
    const float phi = AI_PITIMES2 * rnd_y;

    AtVector U, V;
    AiV3BuildLocalFrame(U, V, N);
    const AtVector wi = sin_theta * cosf(phi) * U
                      + sin_theta * sinf(phi) * V
                      + cosNI_out * N;

    pdf_out = (omc > 0.0f) ? AI_ONEOVER2PI / omc : 0.0f;
    return wi;
}

struct GlossyToonBSDF
{
    AtVector N, Ng, Ns, Rd;
    float size, smooth;
};

AI_BSDF_EXPORT_METHODS(GlossyToonBSDFMtd);

bsdf_init
{
    GlossyToonBSDF* data = (GlossyToonBSDF*)AiBSDFGetData(bsdf);
    data->Ns = (sg->Ngf == sg->Ng) ? sg->Ns : -sg->Ns;
    data->Ng = sg->Ngf;
    data->Rd = sg->Rd;
    toon_setup_params(data->size, data->smooth);

    static const AtBSDFLobeInfo lobe_info[1] = {
        { AI_RAY_SPECULAR_REFLECT, 0, AtString() }
    };
    AiBSDFInitLobes(bsdf, lobe_info, 1);
    AiBSDFInitNormal(bsdf, data->N, true);
}

bsdf_sample
{
    GlossyToonBSDF* data = (GlossyToonBSDF*)AiBSDFGetData(bsdf);
    const AtVector view = -data->Rd;
    const float cosNV   = AiV3Dot(data->N, view);
    if (cosNV <= 0.0f)
        return AI_BSDF_LOBE_MASK_NONE;

    const AtVector R   = AiReflect(-view, data->N);
    const float sangle = toon_sample_angle(data->size, data->smooth);
    const float omc    = one_minus_cos(sangle);

    float cosRI, pdf;
    const AtVector wi = sample_uniform_cone(R, omc, rnd.x, rnd.y, cosRI, pdf);

    if (!(AiV3Dot(wi, data->Ng) > 0.0f) || !(AiV3Dot(wi, data->N) > 0.0f))
        return AI_BSDF_LOBE_MASK_NONE;

    const float angle = acosf(AiMax(cosRI, 0.0f));
    const float intensity = toon_get_intensity(data->size, data->smooth, angle);
    const float bump = AiBSDFBumpShadow(data->Ns, data->N, wi);

    out_wi         = AtVectorDv(wi);
    out_lobe_index = 0;
    out_lobes[0]   = AtBSDFLobeSample(AtRGB(pdf * intensity * bump), 0.0f, pdf);
    return lobe_mask;
}

bsdf_eval
{
    GlossyToonBSDF* data = (GlossyToonBSDF*)AiBSDFGetData(bsdf);
    const float cosNI = AiV3Dot(data->N, wi);
    if (cosNI <= 0.0f)
        return AI_BSDF_LOBE_MASK_NONE;

    const AtVector view = -data->Rd;
    if (AiV3Dot(data->N, view) <= 0.0f)
        return AI_BSDF_LOBE_MASK_NONE;

    const AtVector R   = AiReflect(-view, data->N);
    const float cosRI  = AiV3Dot(R, wi);
    const float sangle = toon_sample_angle(data->size, data->smooth);
    const float angle  = acosf(AiMax(cosRI, 0.0f));
    if (angle >= sangle)
        return AI_BSDF_LOBE_MASK_NONE;

    const float omc       = one_minus_cos(sangle);
    const float pdf       = (omc > 0.0f) ? AI_ONEOVER2PI / omc : 0.0f;
    const float intensity = toon_get_intensity(data->size, data->smooth, angle);
    const float bump      = AiBSDFBumpShadow(data->Ns, data->N, wi);

    out_lobes[0] = AtBSDFLobeSample(AtRGB(pdf * intensity * bump), 0.0f, pdf);
    return lobe_mask;
}

AtBSDF* GlossyToonBSDFCreate(const AtShaderGlobals* sg,
                              const AtVector& N,
                              const AtRGB& color,
                              float size,
                              float smooth)
{
    AtBSDF* bsdf = AiBSDF(sg, color, GlossyToonBSDFMtd, sizeof(GlossyToonBSDF));
    GlossyToonBSDF* data = (GlossyToonBSDF*)AiBSDFGetData(bsdf);
    data->N = N;
    data->size = size;
    data->smooth = smooth;
    return bsdf;
}