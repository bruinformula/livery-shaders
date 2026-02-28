#include "diffuse_ramp_bsdf.h"

// Based on the matching BSDFs from Cycles 

struct DiffuseRampBSDF
{
   /* parameters */
   AtVector N;
   AtArray* colors;
   /* set in bsdf_init */
   AtVector Ng, Ns;
};

AI_BSDF_EXPORT_METHODS(DiffuseRampBSDFMtd);

bsdf_init {
   DiffuseRampBSDF *data = (DiffuseRampBSDF*)AiBSDFGetData(bsdf);

   // store forward facing smooth normal for bump shadowing
   data->Ns = (sg->Ngf == sg->Ng) ? sg->Ns : -sg->Ns;

   // store geometric normal to clip samples below the surface
   data->Ng = sg->Ngf;

   // initialize the BSDF lobes. in this case we just have a single
   // diffuse lobe with no specific flags or label
   static const AtBSDFLobeInfo lobe_info[1] = { {AI_RAY_DIFFUSE_REFLECT, 0, AtString()} };
   AiBSDFInitLobes(bsdf, lobe_info, 1);

   // specify that we will only reflect light in the hemisphere around N
   AiBSDFInitNormal(bsdf, data->N, true);
}

static AtRGB DiffuseRampGetColor(const AtArray* colors, float pos)
{
    const int MAXCOLORS = 8;
    const float npos = pos * (float)(MAXCOLORS - 1);
    const int ipos = (int)npos;
    if (ipos < 0)
        return AiArrayGetRGB(colors, 0);
    if (ipos >= MAXCOLORS - 1)
        return AiArrayGetRGB(colors, MAXCOLORS - 1);
    const float offset = npos - (float)ipos;
    return AiArrayGetRGB(colors, ipos) * (1.0f - offset) +
           AiArrayGetRGB(colors, ipos + 1) * offset;
}

bsdf_sample {
   DiffuseRampBSDF *data = (DiffuseRampBSDF*)AiBSDFGetData(bsdf);

   // sample cosine weighted incoming light direction
   AtVector U, V;
   AiV3BuildLocalFrame(U, V, data->N);
   float sin_theta = sqrtf(rnd.x);
   float phi = 2 * AI_PI * rnd.y;
   float cosNI = sqrtf(1 - rnd.x);
   AtVector wi = sin_theta * cosf(phi) * U +
                 sin_theta * sinf(phi) * V +
                 cosNI * data->N;

   // discard rays below the hemisphere
   if (!(AiV3Dot(wi, data->Ng) > 0))
      return AI_BSDF_LOBE_MASK_NONE;

   const AtRGB rampColor = DiffuseRampGetColor(data->colors, cosNI);
   // since we have perfect importance sampling, the weight (BRDF / pdf) is 1
   // except for the bump shadowing, which is used to avoid artifacts when the
   // shading normal differs significantly from the smooth surface normal
   const float weight = AiBSDFBumpShadow(data->Ns, data->N, wi);

   // pdf for cosine weighted importance sampling
   const float pdf = cosNI * AI_ONEOVERPI;

   // return output direction vectors, we don't compute differentials here
   out_wi = AtVectorDv(wi);

   // specify that we sampled the first (and only) lobe
   out_lobe_index = 0;

   // return weight and pdf
   out_lobes[0] = AtBSDFLobeSample(rampColor * weight, 0.0f, pdf);

   // indicate that we have valid lobe samples for all the requested lobes,
   // which is just one lobe in this case
   return lobe_mask;
}

bsdf_eval
{
   DiffuseRampBSDF *data = (DiffuseRampBSDF*)AiBSDFGetData(bsdf);

   // discard rays below the hemisphere
   const float cosNI = AiV3Dot(data->N, wi);
   if (cosNI <= 0.f)
      return AI_BSDF_LOBE_MASK_NONE;

   // return weight and pdf, same as in bsdf_sample
   const AtRGB rampColor = DiffuseRampGetColor(data->colors, cosNI);
   const float weight = AiBSDFBumpShadow(data->Ns, data->N, wi);
   const float pdf = cosNI * AI_ONEOVERPI;
   out_lobes[0] = AtBSDFLobeSample(rampColor * weight, 0.0f, pdf);

   return lobe_mask;
}

AtBSDF* DiffuseRampBSDFCreate(const AtShaderGlobals* sg, const AtVector& N,
                              AtRGB colorA, AtRGB colorB, AtRGB colorC, AtRGB colorD,
                              AtRGB colorE, AtRGB colorF, AtRGB colorG, AtRGB colorH)
{
   AtBSDF* bsdf = AiBSDF(sg, AtRGB(1.0f), DiffuseRampBSDFMtd, sizeof(DiffuseRampBSDF));
   DiffuseRampBSDF* data = (DiffuseRampBSDF*)AiBSDFGetData(bsdf);
   data->N = N;
   data->colors = AiArray(8, 1, AI_TYPE_RGB,
                          colorA, colorB, colorC, colorD,
                          colorE, colorF, colorG, colorH);
   return bsdf;
}