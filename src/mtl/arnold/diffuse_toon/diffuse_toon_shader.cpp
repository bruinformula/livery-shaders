#include <ai.h>
#include "diffuse_toon_bsdf.h"

// Based on the Toon BSDFs from Blender/Cycles

AI_SHADER_NODE_EXPORT_METHODS(DiffuseToonMtd)

enum DiffuseToonParams {
    p_color,
    p_size,
    p_smooth,
    p_normal
};

node_parameters
{
    AiParameterRGB("color",  0.8f, 0.8f, 0.8f);
    AiParameterFlt("size",   0.5f);
    AiParameterFlt("smooth", 0.0f);
    AiParameterVec("N",      0.0f, 0.0f, 0.0f);
}

node_initialize {}
node_update     {}
node_finish     {}

shader_evaluate
{
    if (sg->Rt & AI_RAY_SHADOW)
        return;

    const AtRGB color  = AiShaderEvalParamRGB(p_color);
    const float size   = AiShaderEvalParamFlt(p_size);
    const float smooth = AiShaderEvalParamFlt(p_smooth);

    AtVector normal = AiShaderEvalParamVec(p_normal);
    if (AiV3IsSmall(normal, 1e-6f))
        normal = sg->Nf;
    else
        normal = AiV3Normalize(normal);

    sg->out.CLOSURE() = DiffuseToonBSDFCreate(sg, normal, color, size, smooth);
}

node_loader
{
    if (i > 0)
        return false;
    node->methods     = DiffuseToonMtd;
    node->output_type = AI_TYPE_CLOSURE;
    node->name        = "diffuse_toon_bsdf";
    node->node_type   = AI_NODE_SHADER;
    strcpy(node->version, AI_VERSION);
    return true;
}